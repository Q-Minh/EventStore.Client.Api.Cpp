#pragma once

#ifndef ES_WRITE_RESULT_HPP
#define ES_WRITE_RESULT_HPP

#include "position.hpp"

namespace es {

class write_result
{
public:
	write_result() = default;

	explicit write_result(std::int64_t next_expected_version, position log_position)
		: next_expected_version_(next_expected_version), log_position_(log_position)
	{}

	std::int64_t next_expected_version() const { return next_expected_version_; }
	position const& log_position() const { return log_position_; }

private:
	std::int64_t next_expected_version_;
	position log_position_;
};

}

#endif // ES_WRITE_RESULT_HPP