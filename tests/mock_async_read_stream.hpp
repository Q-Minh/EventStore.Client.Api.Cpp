#pragma once

#ifndef ES_TEST_MOCK_ASYNC_READ_STREAM_HPP
#define ES_TEST_MOCK_ASYNC_READ_STREAM_HPP

#include <system_error>
#include <utility>

#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/bind_executor.hpp>
#include <asio/buffer.hpp>

namespace es {
namespace test {

struct mock_async_read_stream
{
	asio::io_context::executor_type ex_;
	char* buffer_;
	std::size_t size_;
	asio::io_context::executor_type get_executor() noexcept { return ex_; }

	template <class MutableBufferSequence, class ReadHandler>
	void async_read_some(MutableBufferSequence buffers, ReadHandler&& handler)
	{
		static_assert(
			asio::is_const_buffer_sequence<MutableBufferSequence>::value,
			"argument to parameter 'buffers' must meet MutableBufferSequence requirements"
		);

		std::error_code ec;
		std::size_t bytes_read = 0;
		if (asio::buffer_size(buffers) > 0)
		{
			bytes_read = asio::buffer_copy(std::move(buffers), asio::buffer(buffer_, size_));

			if (bytes_read == 0)
			{
				ec = asio::error::make_error_code(asio::stream_errc::eof);
			}
		}

		asio::post(
			asio::bind_executor(
				ex_,
				std::bind(std::forward<ReadHandler>(handler), ec, bytes_read)
			)
		);
	}
};

}
}

#endif // ES_TEST_MOCK_ASYNC_READ_STREAM_HPP