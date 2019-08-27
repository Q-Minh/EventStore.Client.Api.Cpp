#pragma once

#ifndef ES_GUID_HPP
#define ES_GUID_HPP

#include <string>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/functional/hash.hpp>

namespace es {

using guid_type = boost::uuids::uuid;
using guid_hasher_type = boost::hash<boost::uuids::uuid>;

inline guid_type guid()
{
	static boost::uuids::random_generator gen;
	return gen();
}

inline std::string to_string(guid_type const& uuid)
{
	return boost::uuids::to_string(uuid);
}

inline guid_type guid(std::string_view str)
{
	static boost::uuids::string_generator gen;
	return gen(str.cbegin(), str.cend());
}

// size must be 16
inline guid_type guid(const char* a)
{
	boost::uuids::uuid guid;
	std::memcpy(guid.data, a, guid.size());
	return guid;
}

constexpr inline std::uint8_t size(guid_type const& uuid) { return 16; }
inline char* data(guid_type const& uuid) { return (char*)uuid.data; }

}

// overload std::hash<T>
namespace std {

template <> struct hash<es::guid_type>
{
	std::size_t operator()(const es::guid_type& guid) const
	{
		return boost::hash<es::guid_type>()(guid);
	}
};

} // es

#endif // ES_GUID_HPP