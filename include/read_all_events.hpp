#pragma once

#ifndef ES_READ_ALL_EVENTS_HPP
#define ES_READ_ALL_EVENTS_HPP

#include "all_events_slice.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

template <class ConnectionType, class AllEventsSliceReadHandler>
void async_read_all_events(
	ConnectionType& connection,
	read_direction direction,
	position& from_position,
	int max_count,
	bool resolve_link_tos,
	AllEventsSliceReadHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<AllEventsSliceReadHandler, std::error_code, std::optional<all_events_slice>>,
		"AllEventsSliceReadHandler requirements not met, must have signature R(std::error_code, std::optional<es::all_events_slice>)"
	);

	message::ReadAllEvents request;
	request.set_commit_position(from_position.commit_position());
	request.set_prepare_position(from_position.prepare_position());
	request.set_max_count(max_count);
	request.set_resolve_link_tos(resolve_link_tos);
	request.set_require_master(connection.settings().require_master());

	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection.settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			direction == read_direction::forward ? detail::tcp::tcp_command::read_all_events_forward : detail::tcp::tcp_command::read_all_events_backward,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			direction == read_direction::forward ? detail::tcp::tcp_command::read_all_events_forward : detail::tcp::tcp_command::read_all_events_backward,
			detail::tcp::tcp_flags::authenticated,
			corr_id,
			connection.settings().default_user_credentials().username(),
			connection.settings().default_user_credentials().password(),
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}

	connection.async_send(
		std::move(package),
		[handler = std::move(handler), direction = direction]
	(std::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec)
		{
			if (direction == read_direction::backward &&
				view.command() != detail::tcp::tcp_command::read_all_events_backward_completed)
			{
				ec = make_error_code(communication_errors::unexpected_response);
			}
			if (direction == read_direction::forward && 
				view.command() != detail::tcp::tcp_command::read_all_events_forward_completed)
			{
				ec = make_error_code(communication_errors::unexpected_response);
			}
		}

		if (ec)
		{
			handler(ec, {});
			return;
		}

		message::ReadAllEventsCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.result())
		{
		case message::ReadAllEventsCompleted_ReadAllResult_Success:	
			break;
		case message::ReadAllEventsCompleted_ReadAllResult_Error:
			ec = make_error_code(communication_errors::server_error);
			break;
		case message::ReadAllEventsCompleted_ReadAllResult_AccessDenied:
			ec = make_error_code(stream_errors::access_denied);
			break;
		// do we not deal with NotModified result?
		default:
			ec = make_error_code(communication_errors::unexpected_response);
			break;
		}

		if (!ec)
		{
			auto result = std::make_optional(all_events_slice{
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

} // es

#endif // ES_READ_ALL_EVENTS_HPP