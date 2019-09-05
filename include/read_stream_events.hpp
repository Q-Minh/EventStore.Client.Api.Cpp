#pragma once

#ifndef ES_READ_STREAM_EVENTS_HPP
#define ES_READ_STREAM_EVENTS_HPP

#include "stream_events_slice.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

template <class ConnectionType, class EventsSliceReadHandler>
void async_read_stream_events(
	std::shared_ptr<ConnectionType> const& connection,
	std::string const& stream,
	read_direction direction,
	std::int64_t from_event_number,
	int max_count,
	bool resolve_link_tos,
	EventsSliceReadHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<EventsSliceReadHandler, boost::system::error_code, std::optional<stream_events_slice>>,
		"EventsSliceReadHandler requirements not met, must have signature R(boost::system::error_code, std::optional<es::stream_events_slice>)"
		);

	static message::ReadStreamEvents request;
	request.Clear();
	request.set_event_stream_id(stream);
	request.set_from_event_number(from_event_number);
	request.set_max_count(max_count);
	request.set_require_master(connection->settings().require_master());
	request.set_resolve_link_tos(resolve_link_tos);

	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			direction == read_direction::forward ? detail::tcp::tcp_command::read_stream_events_forward : detail::tcp::tcp_command::read_stream_events_backward,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			direction == read_direction::forward ? detail::tcp::tcp_command::read_stream_events_forward : detail::tcp::tcp_command::read_stream_events_backward,
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
		[handler = std::move(handler), stream = stream, from_event_number = from_event_number, direction = direction]
	(boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec)
		{
			if (direction == read_direction::forward &&
				view.command() != detail::tcp::tcp_command::read_stream_events_forward_completed)
			{
				ec = make_error_code(communication_errors::unexpected_response);
			}
			if (direction == read_direction::backward &&
				view.command() != detail::tcp::tcp_command::read_stream_events_backward_completed)
			{
				ec = make_error_code(communication_errors::unexpected_response);
			}
		}

		if (ec)
		{
			handler(ec, {});
			return;
		}

		static message::ReadStreamEventsCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.result())
		{
		case message::ReadStreamEventsCompleted_ReadStreamResult_Success:
			break;
		case message::ReadStreamEventsCompleted_ReadStreamResult_StreamDeleted:
			ec = make_error_code(stream_errors::stream_deleted);
			break;
		case message::ReadStreamEventsCompleted_ReadStreamResult_NoStream:
			ec = make_error_code(stream_errors::stream_not_found);
			break;
		case message::ReadStreamEventsCompleted_ReadStreamResult_Error:
			ec = make_error_code(communication_errors::server_error);
			break;
		case message::ReadStreamEventsCompleted_ReadStreamResult_AccessDenied:
			ec = make_error_code(stream_errors::access_denied);
			break;
		default:
			ec = make_error_code(communication_errors::unexpected_response);
		}

		if (!ec)
		{
			auto result = std::make_optional(stream_events_slice{
				stream,
				from_event_number,
				direction,
				response
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

#endif // ES_READ_STREAM_EVENTS_HPP