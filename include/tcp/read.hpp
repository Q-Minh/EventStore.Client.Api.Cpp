#pragma once

#ifndef RECEIVE_TCP_PACKAGE_OPERATIONS_HPP
#define RECEIVE_TCP_PACKAGE_OPERATIONS_HPP

#include <memory>
#include <cstddef>

#include <boost/asio/read.hpp>

#include "logger.hpp"
#include "tcp/tcp_package.hpp"

namespace es {
namespace tcp {
namespace operations {

template <class ConnectionType, class DynamicBuffer, class CompletionToken>
class read_message_handler
{
public:
	using connection_type = ConnectionType;
	using dynamic_buffer_type = DynamicBuffer;

	explicit read_message_handler(
		std::shared_ptr<connection_type> const& connection,
		dynamic_buffer_type& buffer,
		CompletionToken&& token
	) : connection_(connection), buffer_(buffer), token_(std::forward<CompletionToken>(token))
	{}

	void operator()(boost::system::error_code ec, std::size_t bytes_read)
	{
		if (connection_.expired())
		{
			buffer_.consume(4);
			return;
		}

		// regardless of error code, call token
		if (ec)
		{
			ES_ERROR("failed to read tcp frame message, {}", ec.message());
			buffer_.consume(4);
		}
		else
		{
			buffer_.commit(bytes_read);
		}

		token_(ec, bytes_read + 4);
	}
private:
	std::weak_ptr<connection_type> connection_;
	dynamic_buffer_type& buffer_;
	CompletionToken token_;
};

template <class ConnectionType, class DynamicBuffer, class CompletionToken>
class length_check_handler
{
public:
	using connection_type = ConnectionType;
	using dynamic_buffer_type = DynamicBuffer;

	explicit length_check_handler(
		std::shared_ptr<connection_type> const& connection,
		dynamic_buffer_type& buffer,
		CompletionToken&& token
	) : connection_(connection), buffer_(buffer), token_(std::forward<CompletionToken>(token))
	{}

	void operator()(boost::system::error_code ec, std::size_t bytes_read)
	{
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		if (!ec)
		{
			auto* ptr = const_cast<void*>(buffer_.data().data());
			int length = *reinterpret_cast<int*>(ptr);
				
			if (length < detail::tcp::kMandatorySize - detail::tcp::kCommandOffset || length > detail::tcp::kMaxPackageSize)
			{
				ec = make_error_code(communication_errors::bad_length_prefix);
				token_(ec, bytes_read);
				return;
			}

			// commit length prefix
			buffer_.commit(4);

			boost::asio::async_read(
				conn->socket(),
				//boost::asio::buffer(buffer_ + 4, length),
				buffer_.prepare(length),
				read_message_handler<connection_type, dynamic_buffer_type, CompletionToken>(conn, buffer_, std::move(token_))
			);
			return;
		}
		else
		{
			ES_ERROR("failed to read tcp frame length, {}", ec.message());
			token_(ec, bytes_read);
			return;
		}
	}
private:
	std::weak_ptr<connection_type> connection_;
	dynamic_buffer_type& buffer_;
	CompletionToken token_;
};

template <class ConnectionType, class DynamicBuffer>
class read_tcp_package_op
{
public:
	using connection_type = ConnectionType;
	using dynamic_buffer_type = DynamicBuffer;

	explicit read_tcp_package_op(
		std::shared_ptr<connection_type> const& connection,
		dynamic_buffer_type& buffer
	) : connection_(connection), buffer_(buffer)
	{}

	template <class CompletionToken>
	void initiate(CompletionToken&& token)
	{
		// set error message !!
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		length_check_handler<connection_type, dynamic_buffer_type, CompletionToken> handler(conn, buffer_, std::move(token));

		// not sure if this is what we want, maybe we want more flexiblity, this works well for single threaded code ...
		buffer_.consume(buffer_.size());

		boost::asio::async_read(
			conn->socket(),
			/*boost::asio::buffer(buffer_, 4),*/
			buffer_.prepare(4),
			std::move(handler)
		);
	}
private:
	std::weak_ptr<connection_type> connection_;
	dynamic_buffer_type& buffer_;
};

} // operations
} // tcp
} // es

#endif // RECEIVE_TCP_PACKAGE_OPERATIONS_HPP