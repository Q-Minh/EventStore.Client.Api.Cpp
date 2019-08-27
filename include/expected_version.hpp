#pragma once

#ifndef ES_EXPECTED_VERSION_HPP
#define ES_EXPECTED_VERSION_HPP

namespace es {

enum class expected_version : long
{
	any = -2,
	no_stream = -1,
	stream_exists = -4
};

}

#endif // ES_EXPECTED_VERSION_HPP