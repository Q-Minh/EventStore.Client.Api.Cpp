#pragma once

#ifndef ES_RESOLVED_EVENT_HPP
#define ES_RESOLVED_EVENT_HPP

#include <optional>

#include "position.hpp"
#include "recorded_event.hpp"

namespace es {

class resolved_event
{
public:
	explicit resolved_event(
		message::ResolvedEvent& event
	) : event_(*event.mutable_event()),
		link_(),
		original_position_(position{ event.commit_position(), event.prepare_position() })
	{
		if (event.has_link())
		{
			link_.emplace(*event.mutable_link());
		}
	}

	explicit resolved_event(
		message::ResolvedIndexedEvent& event
	) : event_(*event.mutable_event()),
		link_(),
		original_position_()
	{
		if (event.has_link())
		{
			link_.emplace(*event.mutable_link());
		}
	}

	// might be changed to standard optional
	recorded_event const& event() const { return event_; }

	std::optional<recorded_event> const& link() const { return link_; }
	recorded_event const& original_event() const { return link_.has_value() ? link_.value() : event_; }
	bool is_resolved() const { return link_.has_value(); } // && event_.has_value() if event_ is optional
	std::optional<position> original_position() const { return original_position_; }
	std::string const& original_stream_id() const { return original_event().stream_id(); }
	std::int64_t original_event_number() const { return original_event().event_number(); }

private:
	// in .NET client api, event_ might be null, i wonder why since it is required in proto file
	recorded_event event_;
	std::optional<recorded_event> link_;
	std::optional<position> original_position_;
};

}

#endif // ES_RESOLVED_EVENT_HPP