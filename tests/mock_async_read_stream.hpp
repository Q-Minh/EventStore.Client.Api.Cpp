#pragma once

#ifndef ES_TEST_MOCK_ASYNC_READ_STREAM_HPP
#define ES_TEST_MOCK_ASYNC_READ_STREAM_HPP

#include <boost/system/error_code.hpp>
#include <utility>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/buffer.hpp>

namespace es {
namespace test {

struct mock_async_read_stream
{
	boost::asio::io_context::executor_type ex_;
	char* buffer_;
	std::size_t size_;
	boost::asio::io_context::executor_type get_executor() noexcept { return ex_; }

	template <class MutableBufferSequence, class ReadHandler>
	void async_read_some(MutableBufferSequence buffers, ReadHandler&& handler)
	{
		static_assert(
			boost::asio::is_const_buffer_sequence<MutableBufferSequence>::value,
			"argument to parameter 'buffers' must meet MutableBufferSequence requirements"
		);

		boost::system::error_code ec;
		std::size_t bytes_read = 0;
		if (boost::asio::buffer_size(buffers) > 0)
		{
			bytes_read = boost::asio::buffer_copy(std::move(buffers), boost::asio::buffer(buffer_, size_));

			if (bytes_read == 0)
			{
				ec = boost::asio::error::make_error_code(boost::asio::stream_errc::eof);
			}
		}

		boost::asio::post(
			boost::asio::bind_executor(
				ex_,
				std::bind(std::forward<ReadHandler>(handler), ec, bytes_read)
			)
		);
	}
};

}
}

#endif // ES_TEST_MOCK_ASYNC_READ_STREAM_HPP