#pragma once

#ifndef ES_READ_RESULT_HPP
#define ES_READ_RESULT_HPP

#include "resolved_event.hpp"

namespace es {

enum class event_read_status
{
	success = 0,
	not_found = 1,
	no_stream = 2,
	stream_deleted = 3
};

class event_read_result
{
public:
	explicit event_read_result(
		event_read_status status,
		std::string const& stream,
		std::int64_t event_number,
		message::ResolvedIndexedEvent event
	) : status_(status),
		stream_(stream),
		event_number_(event_number),
		event_()
	{
		if (status_ == event_read_status::success)
		{
			event_.emplace(event);
		}
	}

	event_read_status status() const { return status_; }
	std::string const& stream() const { return stream_; }
	std::int64_t event_number() const { return event_number_; }
	std::optional<resolved_event> const& event() const { return event_; }

private:
	event_read_status status_;
	std::string stream_;
	std::int64_t event_number_;
	std::optional<resolved_event> event_;
};

}

#endif // ES_READ_RESULT_HPP