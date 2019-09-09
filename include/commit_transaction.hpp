#pragma once

#ifndef ES_COMMIT_TRANSACTION_HPP
#define ES_COMMIT_TRANSACTION_HPP

#include "message/messages.pb.h"

#include "guid.hpp"
#include "write_result.hpp"
#include "tcp/handle_operation_error.hpp"

namespace es {

template <class ConnectionType, class WriteResultHandler>
void async_commit_transaction(
	std::shared_ptr<ConnectionType> const& connection,
	std::int64_t transaction_id,
	WriteResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<WriteResultHandler, boost::system::error_code, std::optional<write_result>>,
		"WriteResultHandler requirements not met, must have signature R(boost::system::error_code, std::optional<es::write_result>)"
	);

	message::TransactionCommit request;
	request.set_transaction_id(transaction_id);
	request.set_require_master(connection->settings().require_master());

	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::transaction_commit,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::transaction_commit,
			detail::tcp::tcp_flags::authenticated,
			corr_id,
			connection->settings().default_user_credentials().username(),
			connection->settings().default_user_credentials().password(),
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}

	connection->async_send(
		std::move(package),
		[&ioc = connection->get_io_context(), handler = std::move(handler), connection = connection]
		(boost::system::error_code ec, detail::tcp::tcp_package_view view)
		{
			if (!ec && view.command() != detail::tcp::tcp_command::transaction_commit_completed)
			{
				//ec = make_error_code(communication_errors::unexpected_response);
				auto& discovery_service = boost::asio::use_service<typename ConnectionType::discovery_service_type>(ioc);
				detail::handle_operation_error(ec, view, discovery_service);
			}

			if (ec)
			{
				handler(ec, {});
				return;
			}

			message::TransactionCommitCompleted response;
			response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

			switch (response.result())
			{
			case message::OperationResult::Success:
				break;
			case message::OperationResult::PrepareTimeout:
				// retry
				ec = make_error_code(operation_errors::server_timeout);
				return;
			case message::OperationResult::ForwardTimeout:
				// retry
				ec = make_error_code(operation_errors::server_timeout);
				return;
			case message::OperationResult::CommitTimeout:
				// retry
				ec = make_error_code(operation_errors::server_timeout);
				return;
			case message::OperationResult::WrongExpectedVersion:
				ec = make_error_code(stream_errors::wrong_expected_version);
				break;
			case message::OperationResult::StreamDeleted:
				ec = make_error_code(stream_errors::stream_deleted);
				break;
			case message::OperationResult::InvalidTransaction:
				ec = make_error_code(stream_errors::invalid_transaction);
				break;
			case message::OperationResult::AccessDenied:
				ec = make_error_code(stream_errors::access_denied);
				break;
			default:
				ec = make_error_code(communication_errors::unexpected_response);
				break;
			}

			if (!ec)
			{
				std::int64_t prepare_position = -1;
				std::int64_t commit_position = -1;
				if (response.has_prepare_position()) prepare_position = response.prepare_position();
				if (response.has_commit_position()) commit_position = response.commit_position();

				auto result = std::make_optional(write_result{
						response.last_event_number(),
						// prepare position is put as argument to commit position, and vice versa, 
						// is this expected behavior (see .NET client api)?
						position{ prepare_position, commit_position }
					});

				handler(ec, std::move(result));
				return;
			}
			else
			{
				handler(ec, {});
				return;
			}
		}
	);
}

}

#endif // ES_COMMIT_TRANSACTION_HPP