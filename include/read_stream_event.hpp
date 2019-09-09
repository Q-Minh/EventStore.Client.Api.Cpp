#pragma once

/*
Add user credentials parameter to have authentication per request
*/

#ifndef ES_READ_STREAM_EVENT_HPP
#define ES_READ_STREAM_EVENT_HPP

#include <memory>

#include "event_read_result.hpp"
#include "tcp/handle_operation_error.hpp"

namespace es {

template <class ConnectionType, class EventReadResultHandler>
void async_read_stream_event(
	std::shared_ptr<ConnectionType> const& connection,
	std::string const& stream,
	std::int64_t event_number,
	bool resolveLinkTos,
	EventReadResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<EventReadResultHandler, boost::system::error_code, std::optional<event_read_result>>,
		"ReadResultHandler requirements not met, must have signature R(boost::system::error_code, std::optional<es::event_read_result>)"
	);

	static message::ReadEvent request;
	request.Clear();
	request.set_event_stream_id(stream);
	request.set_event_number(event_number);
	request.set_resolve_link_tos(resolveLinkTos);
	request.set_require_master(connection->settings().require_master());

	auto serialized = request.SerializeAsString();

	auto corr_id = es::guid();
	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::read_event,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::read_event,
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
		[&ioc = connection->get_io_context(), handler = std::move(handler), stream = stream, event_number = event_number](boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec && view.command() != detail::tcp::tcp_command::read_event_completed)
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

		static message::ReadEventCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());
		
		event_read_status status;
		switch (response.result())
		{
		// why the ugly long names ?? should see protoc and regenerate files
		case message::ReadEventCompleted_ReadEventResult_Success:
			status = event_read_status::success;
			break;
		case message::ReadEventCompleted_ReadEventResult_NotFound:
			status = event_read_status::not_found;
			break;
		case message::ReadEventCompleted_ReadEventResult_NoStream:
			status = event_read_status::no_stream;
			break;
		case message::ReadEventCompleted_ReadEventResult_StreamDeleted:
			status = event_read_status::stream_deleted;
			break;
		case message::ReadEventCompleted_ReadEventResult_Error:
			ec = make_error_code(communication_errors::server_error);
			break;
		case message::ReadEventCompleted_ReadEventResult_AccessDenied:
			ec = make_error_code(stream_errors::access_denied);
			break;
		default:
			ec = make_error_code(communication_errors::unexpected_response);
			break;
		}

		if (!ec)
		{
			auto result = std::make_optional(event_read_result{ status, stream, event_number, response.event() });
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

#endif // ES_READ_STREAM_EVENT_HPP