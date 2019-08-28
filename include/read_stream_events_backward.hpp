#pragma once

#ifndef ES_READ_STREAM_EVENTS_BACKWARD_HPP
#define ES_READ_STREAM_EVENTS_BACKWARD_HPP

namespace es {

template <class ConnectionType, class EventsSliceReadHandler>
void async_read_stream_events_backward(
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
		read_direction::backward,
		from_event_number,
		max_count,
		resolve_link_tos,
		std::forward<EventsSliceReadHandler>(handler)
	);
}

}

#endif // ES_READ_STREAM_EVENTS_BACKWARD_HPP