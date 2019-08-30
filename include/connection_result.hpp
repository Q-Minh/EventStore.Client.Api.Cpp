#pragma once

#ifndef ES_CONNECTION_RESULT_HPP
#define ES_CONNECTION_RESULT_HPP

#include "guid.hpp"

namespace es {

class connection_result
{
public:
	explicit connection_result(
		guid_type const& connection_id
	) : connection_id_(connection_id)
	{}

	guid_type const& connection_id() const { return connection_id_; }

private:
	guid_type connection_id_;
};

}

#endif // ES_CONNECTION_RESULT_HPP