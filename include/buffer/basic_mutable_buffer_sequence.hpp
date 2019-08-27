#pragma once

#ifndef ES_BASIC_MUTABLE_BUFFER_SEQUENCE_HPP
#define ES_BASIC_MUTABLE_BUFFER_SEQUENCE_HPP

#include <cstdint>

#include <asio/buffer.hpp>

namespace es {
namespace buffer {

class basic_mutable_buffer_sequence
{
public:
	asio::mutable_buffers_1* begin() { return &mutable_buffer_; }
	asio::mutable_buffers_1* end() { return &mutable_buffer_ + 1; }

private:
	std::uint8_t buffer_[1 << 16];
	asio::mutable_buffers_1 mutable_buffer_ = asio::buffer(buffer_, 1 << 16);
};

}
}

#endif // ES_BASIC_MUTABLE_BUFFER_SEQUENCE_HPP