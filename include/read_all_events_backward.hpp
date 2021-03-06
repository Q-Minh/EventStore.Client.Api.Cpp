#pragma once

#ifndef ES_READ_ALL_EVENTS_BACKWARD_HPP
#define ES_READ_ALL_EVENTS_BACKWARD_HPP

#include "read_all_events.hpp"

namespace es {

template <class ConnectionType, class AllEventsSliceReadHandler>
void async_read_all_events_backward(
	std::shared_ptr<ConnectionType> const& connection,
	position& from_position,
	int max_count,
	bool resolve_link_tos,
	AllEventsSliceReadHandler&& handler
)
{
	async_read_all_events(
		connection,
		read_direction::backward,
		from_position,
		max_count,
		resolve_link_tos,
		std::forward<AllEventsSliceReadHandler>(handler)
	);
}

}

#endif // ES_READ_ALL_EVENTS_BACKWARD_HPP