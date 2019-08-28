#pragma once

#ifndef ES_STREAM_EVENTS_SLICE_HPP
#define ES_STREAM_EVENTS_SLICE_HPP

#include "read_direction.hpp"
#include "resolved_event.hpp"

namespace es {

class stream_events_slice
{
public:
	explicit stream_events_slice(
		std::string const& stream,
		std::int64_t from_event_number,
		read_direction direction,
		message::ReadStreamEventsCompleted& response
	) : from_event_number_(from_event_number),
		next_event_number_(response.next_event_number()),
		last_event_number_(response.last_event_number()),
		stream_(stream),
		read_direction_(direction),
		is_end_of_stream_(response.is_end_of_stream()),
		events_()
	{
		// preallocate
		int size = response.events_size();
		events_.reserve(size);

		// convert all proto events to user events
		auto events = response.events();
		std::transform(
			std::begin(events),
			std::end(events),
			std::back_inserter(events_),
			[](message::ResolvedIndexedEvent& event) -> resolved_event { return resolved_event(event); }
		);
	}

	std::string const& stream() const { return stream_; }
	std::int64_t from_event_number() const { return from_event_number_; }
	std::int64_t next_event_number() const { return next_event_number_; }
	std::int64_t last_event_number() const { return last_event_number_; }
	read_direction stream_read_direction() const { return read_direction_; }
	bool is_end_of_stream() const { return is_end_of_stream_; }
	std::vector<resolved_event> const& events() const { return events_; }

private:
	std::int64_t from_event_number_;
	std::int64_t next_event_number_;
	std::int64_t last_event_number_;
	std::string stream_;
	read_direction read_direction_;
	bool is_end_of_stream_;
	std::vector<resolved_event> events_; // is there an alternative structure for these?
};

}

#endif // ES_STREAM_EVENTS_SLICE_HPP