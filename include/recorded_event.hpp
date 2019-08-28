#pragma once

#ifndef ES_RECORDED_EVENT_HPP
#define ES_RECORDED_EVENT_HPP

#include <string>
#include <utility>

#include "message/messages.pb.h"

#include "guid.hpp"

namespace es {

class recorded_event
{
public:
	// record will be invalid after this constructor
	explicit recorded_event(message::EventRecord& record)
		: event_stream_id_(),
		  event_id_(es::guid(record.event_id().data())),
		  event_number_(record.event_number()),
		  event_type_(),
		  content_(),
		  metadata_(),
		  is_json_(record.data_content_type() == 1),
		  created_(record.created()), // maybe check for optional ?
		  created_epoch_(record.created_epoch()) // maybe check for optional ?
	{
		using std::swap;
		
		swap(*record.mutable_event_stream_id(), event_stream_id_);
		swap(*record.mutable_event_type(), event_type_);
		swap(*record.mutable_data(), content_);
		swap(*record.mutable_metadata(), metadata_);
	}

	std::string const& stream_id() const { return event_stream_id_; }
	guid_type const& event_id() const { return event_id_; }
	std::int64_t event_number() const { return event_number_; }
	std::string const& event_type() const { return event_type_; }
	std::string const& content() const { return content_; }
	std::string const& metadata() const { return metadata_; }
	bool is_json() const { return is_json_; }
	std::int64_t created() const { return created_; } // maybe use optional
	std::int64_t created_epoch() const { return created_epoch_; } // maybe use optional

private:
	std::string event_stream_id_;
	guid_type event_id_;
	std::int64_t event_number_;
	std::string event_type_;
	std::string content_;
	std::string metadata_;
	bool is_json_;
	std::int64_t created_; // find a way to convert to time_point or something later
	std::int64_t created_epoch_;
};

}

#endif // ES_RECORDED_EVENT_HPP