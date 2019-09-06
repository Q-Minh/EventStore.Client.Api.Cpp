#pragma once

#ifndef ES_DYNAMIC_BUFFER_HPP
#define ES_DYNAMIC_BUFFER_HPP

#include <numeric>
#include <vector>
#include <boost/asio/buffer.hpp>

namespace es {
namespace buffer {

template <class T, class Allocator = std::allocator<T>>
class dynamic_buffer : public boost::asio::dynamic_vector_buffer<T, Allocator>
{
public:
	using base_type = boost::asio::dynamic_vector_buffer<T, Allocator>;

	explicit dynamic_buffer(std::size_t maximum_size =
		(std::numeric_limits<std::size_t>::max)()) noexcept
		: base_type(storage_, maximum_size)
	{}

	dynamic_buffer(dynamic_buffer&& other) noexcept
		: base_type(storage_), storage_(std::move(other.storage_))
	{}
private:
	std::vector<T, Allocator> storage_;
};

}
}

#endif // ES_DYNAMIC_BUFFER_HPP