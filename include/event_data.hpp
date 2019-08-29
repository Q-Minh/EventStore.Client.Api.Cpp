#pragma once

#ifndef ES_EVENT_DATA_HPP
#define ES_EVENT_DATA_HPP

#include <string>
#include <optional>

#include "guid.hpp"

namespace es {

class event_data
{
public:
	event_data() = default;

	explicit event_data(
		guid_type const& event_id,
		std::string const& type,
		bool is_json,
		std::string const& content,
		std::optional<std::string> const& metadata
	) : event_id_(event_id),
		type_(type),
		is_json_(is_json),
		content_(content),
		metadata_(metadata)
	{}
	
	guid_type const& event_id() const { return event_id_; }
	std::string const& type() const { return type_; }
	bool is_json() const { return is_json_; }
	std::string const& content() const { return content_; }
	std::optional<std::string> const& metadata() const { return metadata_; }

	// can be moved from
	guid_type& event_id() { return event_id_; }
	std::string& type() { return type_; }
	void set_is_json(bool is_json) { is_json_ = is_json; }
	std::string& content() { return content_; }
	std::optional<std::string>& metadata() { return metadata_; }

private:
	guid_type event_id_;
	std::string type_;
	bool is_json_;
	std::string content_;
	std::optional<std::string> metadata_;
};

}

#endif // ES_EVENT_DATA_HPP