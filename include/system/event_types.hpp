#pragma once

#ifndef ES_EVENT_TYPES_HPP
#define ES_EVENT_TYPES_HPP

namespace es {
namespace system {

struct event_types
{
	inline static const char stream_deleted[] = "$streamDeleted";
	inline static const char stats_collection[] = "$statsCollected";
	inline static const char link_to[] = "$>";
	inline static const char stream_metadata[] = "$metadata";
	inline static const char settings[] = "$settings";
};

}
}

#endif // ES_EVENT_TYPES_HPP