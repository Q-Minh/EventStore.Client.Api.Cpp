#pragma once

#ifndef ES_READ_STREAM_EVENTS_FORWARD_HPP
#define ES_READ_STREAM_EVENTS_FORWARD_HPP

#include "read_stream_events.hpp"

namespace es {

template <class ConnectionType, class EventsSliceReadHandler>
void async_read_stream_events_forward(
	ConnectionType& connection,
	std::string const& stream,
	std::int64_t from_event_number,
	int max_count,
	bool resolve_link_tos,
	EventsSliceReadHandler&& handler
)
{
	async_read_stream_events(
		connection,
		stream,
		read_direction::forward,
		from_event_number,
		max_count,
		resolve_link_tos,
		std::forward<EventsSliceReadHandler>(handler)
	);
}

}

#endif // ES_READ_STREAM_EVENTS_FORWARD_HPP