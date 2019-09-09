#pragma once

#ifndef ES_START_TRANSACTION_HPP
#define ES_START_TRANSACTION_HPP

#include "message/messages.pb.h"

#include "guid.hpp"
#include "transaction.hpp"
#include "tcp/handle_operation_error.hpp"

namespace es {

template <class ConnectionType, class TransactionStartedHandler>
void async_start_transaction(
	std::shared_ptr<ConnectionType> const& connection,
	std::string const& stream,
	std::int64_t expected_version,
	TransactionStartedHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<TransactionStartedHandler, boost::system::error_code, std::optional<transaction<ConnectionType>>>,
		"TransactionStartedHandler requirements not met, must have signature R(boost::system::error_code, std::optional<es::transaction<ConnectionType>>)"
	);

	message::TransactionStart request;
	request.set_event_stream_id(stream);
	request.set_expected_version(expected_version);
	request.set_require_master(connection->settings().require_master());
	
	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::transaction_start,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::transaction_start,
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
		[&ioc = connection->get_io_context(), handler = std::move(handler), stream = stream, connection = connection]
		(boost::system::error_code ec, detail::tcp::tcp_package_view view)
		{
			if (!ec && view.command() != detail::tcp::tcp_command::transaction_start_completed)
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

			message::TransactionStartCompleted response;
			response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

			switch (response.result())
			{
			case message::OperationResult::Success:
				break;
			case message::OperationResult::PrepareTimeout:
				// retry
				break;
			case message::OperationResult::CommitTimeout:
				// retry
				break;
			case message::OperationResult::ForwardTimeout:
				// retry
				break;
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
				auto result = std::make_optional(transaction{ response.transaction_id(), connection });
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

#endif // ES_START_TRANSACTION_HPP