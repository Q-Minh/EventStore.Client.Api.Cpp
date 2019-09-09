#pragma once

#ifndef ES_DELETE_STREAM_HPP
#define ES_DELETE_STREAM_HPP

#include <string>
#include <optional>

#include "message/messages.pb.h"

#include "delete_stream_result.hpp"
#include "guid.hpp"
#include "tcp/handle_operation_error.hpp"

namespace es {

template <class ConnectionType, class DeleteResultHandler>
void async_delete_stream(
	std::shared_ptr<ConnectionType> const& connection,
	std::string const& stream,
	std::int64_t expected_version,
	DeleteResultHandler&& handler,
	bool hard_delete = false
)
{
	static_assert(
		std::is_invocable_v<DeleteResultHandler, boost::system::error_code, std::optional<delete_stream_result>>,
		"DeleteResultHandler requirements not met, must have signature R(boost::system::error_code, es::delete_stream_result)"
	);

	message::DeleteStream request;
	request.set_event_stream_id(stream);
	request.set_expected_version(expected_version);
	request.set_hard_delete(hard_delete);
	request.set_require_master(connection->settings().require_master());

	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::delete_stream,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::delete_stream,
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
		[&ioc = connection->get_io_context(), handler = std::move(handler)](boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec && view.command() != detail::tcp::tcp_command::delete_stream_completed)
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

		message::DeleteStreamCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.result())
		{
		case message::OperationResult::Success:
			break;
		case message::OperationResult::PrepareTimeout:
			// retry
			ec = make_error_code(operation_errors::server_timeout);
			break;
		case message::OperationResult::CommitTimeout:
			// retry
			ec = make_error_code(operation_errors::server_timeout);
			break;
		case message::OperationResult::ForwardTimeout:
			// retry
			ec = make_error_code(operation_errors::server_timeout);
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
			std::int64_t prepare_position = -1;
			std::int64_t commit_position = -1;
			if (response.has_prepare_position()) prepare_position = response.prepare_position();
			if (response.has_commit_position()) commit_position = response.commit_position();

			auto result = std::make_optional(delete_stream_result{ position(prepare_position, commit_position) });
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

#endif // ES_DELETE_STREAM_HPP