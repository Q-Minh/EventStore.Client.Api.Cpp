#pragma once

#ifndef ES_SET_STREAM_METADATA_HPP
#define ES_SET_STREAM_METADATA_HPP

#include "event_data.hpp"
#include "append_to_stream.hpp"
#include "system/event_types.hpp"
#include "system/streams.hpp"

namespace es {

// assumes metadata is in json, seems it is an EventStore requirement 
// on metadata format, but maybe I'm wrong
template <class ConnectionType, class WriteResultHandler>
void async_set_stream_metadata(
	std::shared_ptr<ConnectionType> const& connection,
	std::string const& stream,
	std::int64_t expected_metastream_version,
	std::string const& metadata, // maybe add a movable parameter for performance if metadata is a non-trivial sized string
	WriteResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<WriteResultHandler, std::error_code, std::optional<write_result>>,
		"WriteResultHandler requirements not met, must have signature R(std::error_code, std::optional<es::write_result>)"
	);

	std::vector<event_data> events{};
	events.emplace_back(event_data{ es::guid(), system::event_types::stream_metadata, true, metadata, {} });

	async_append_to_stream(
		connection,
		system::streams::meta_stream_of(stream),
		std::move(events),
		std::forward<WriteResultHandler>(handler),
		expected_metastream_version
	);
}

}

#endif // ES_SET_STREAM_METADATA_HPP