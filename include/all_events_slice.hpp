#pragma once

#ifndef ES_ALL_EVENTS_SLICE_HPP
#define ES_ALL_EVENTS_SLICE_HPP

#include "position.hpp"
#include "read_direction.hpp"
#include "resolved_event.hpp"

namespace es {

class all_events_slice
{
public:
	explicit all_events_slice(
		read_direction direction,
		message::ReadAllEventsCompleted& response
	) : from_position_(position{ response.commit_position(), response.prepare_position() }),
		next_position_(position{ response.next_commit_position(), response.next_prepare_position() }),
		direction_(direction),
		is_end_of_stream_(),
		events_()
	{
		auto size = response.events_size();
		is_end_of_stream_ = size == 0;

		// preallocate
		events_.reserve(size);

		auto events = response.events();
		std::transform(
			std::begin(events),
			std::end(events),
			std::back_inserter(events_),
			[](message::ResolvedEvent& event) -> resolved_event { return resolved_event(event); }
		);
	}

	position const& from_position() const { return from_position_; }
	position const& next_position() const { return next_position_; }
	read_direction stream_read_direction() const { return direction_; }
	bool is_end_of_stream() const { return is_end_of_stream_; }
	std::vector<resolved_event> const& events() const { return events_; }

private:
	position from_position_;
	position next_position_;
	read_direction direction_;
	bool is_end_of_stream_;
	std::vector<resolved_event> events_;
};

}

#endif // ES_ALL_EVENTS_SLICE_HPP