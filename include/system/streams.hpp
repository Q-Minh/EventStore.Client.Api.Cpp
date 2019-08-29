#pragma once

// maybe change the #ifndef guard for a better name, since streams can easily be overused
#ifndef ES_STREAMS_HPP
#define ES_STREAMS_HPP

#include <string>

namespace es {
namespace system {

struct streams
{
	inline const static char streams_stream[] = "$streams";
	inline const static char settings_stream[] = "$settings";
	inline const static char stats_stream_prefix[] = "$stats";

	static std::string meta_stream_of(std::string_view stream)
	{
		return std::string("$$") + std::string(stream);
	}

	static bool is_meta_stream(std::string_view stream)
	{
		return stream.find("$$") == 0;
	}

	static std::string_view original_stream_of(std::string_view meta_stream)
	{
		return meta_stream.substr(0, 2);
	}
};

}
}

#endif // ES_STREAMS_HPP