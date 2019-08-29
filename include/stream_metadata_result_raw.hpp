#pragma once

#ifndef ES_STREAM_METADATA_RESULT_RAW_HPP
#define ES_STREAM_METADATA_RESULT_RAW_HPP

#include <string>

namespace es {
	
class stream_metadata_result_raw
{
public:
	explicit stream_metadata_result_raw(
		std::string const& stream,
		bool is_stream_deleted,
		std::int64_t meta_stream_version,
		std::string&& stream_metadata
	) : stream_(stream),
		is_stream_deleted_(is_stream_deleted),
		meta_stream_version_(meta_stream_version),
		stream_metadata_(std::move(stream_metadata))
	{}

	explicit stream_metadata_result_raw(stream_metadata_result_raw&& other) = default;

	std::string const& stream() const { return stream_; }
	bool is_stream_deleted() const { return is_stream_deleted_; }
	std::int64_t meta_stream_version() const { return meta_stream_version_; }
	std::string const& stream_metadata() const { return stream_metadata_; }

private:
	std::string stream_;
	bool is_stream_deleted_;
	std::int64_t meta_stream_version_;
	std::string stream_metadata_;
};

}

#endif // ES_STREAM_METADATA_RESULT_RAW_HPP