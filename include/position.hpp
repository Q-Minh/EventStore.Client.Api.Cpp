#pragma once

#ifndef ES_POSITION_HPP
#define ES_POSITION_HPP

#include <cstdint>

namespace es {

class position
{
public:
	position() : commit_position_(-1), prepare_position_(-1) {}

	explicit position(std::int64_t commit_position, std::int64_t prepare_position)
		: commit_position_(commit_position), prepare_position_(prepare_position)
	{}

	static position start() { return position(0, 0); }
	static position end() { return position(-1, -1); }

	std::int64_t commit_position() const { return commit_position_; }
	std::int64_t prepare_position() const { return prepare_position_; }

	bool operator<(position const& other) const
	{
		return commit_position_ < other.commit_position_ ||
			(commit_position_ == other.commit_position_ && prepare_position_ < other.prepare_position_);
	}

	bool operator>(position const& other) const
	{
		return commit_position_ > other.commit_position_ ||
			(commit_position_ == other.commit_position_ && prepare_position_ > other.prepare_position_);
	}

	bool operator==(position const& other) const
	{
		return commit_position_ == other.commit_position_ && prepare_position_ == other.prepare_position_;
	}

	bool operator!=(position const& other) const
	{
		return !(this->operator==(other));
	}

	bool operator>=(position const& other) const
	{
		return (*this) == other || (*this) > other;
	}

	bool operator <=(position const& other) const
	{
		return (*this) == other || (*this) < other;
	}

private:
	std::int64_t commit_position_;
	std::int64_t prepare_position_;
};

}

#endif // ES_POSITION_HPP