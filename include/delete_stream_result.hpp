#pragma once

#ifndef ES_DELETE_STREAM_RESULT_HPP
#define ES_DELETE_STREAM_RESULT_HPP

#include "position.hpp"

namespace es {

class delete_stream_result
{
public:
	explicit delete_stream_result(position log_position) : log_position_(log_position) {}

	position const& log_position() const { return log_position_; }

private:
	position log_position_;
};

}

#endif // ES_DELETE_STREAM_RESULT_HPP