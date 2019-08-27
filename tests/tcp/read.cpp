#include <catch2/catch.hpp>

#include <system_error>
#include <memory>

#include <asio/io_context.hpp>
#include <asio/buffer.hpp>

#include "mock_tcp_connection.hpp"
#include "mock_async_read_stream.hpp"

#include "error/error.hpp"
#include "tcp/read.hpp"
#include "tcp/tcp_package.hpp"

TEST_CASE("read operations on es tcp connections", "[tcp][read]")
{
	const int size = 1 << 16;

	using dynamic_buffer_type = asio::dynamic_vector_buffer<std::uint8_t, std::allocator<std::uint8_t>>;
	std::vector<std::uint8_t> buffer_storage;
	dynamic_buffer_type dynamic_buffer{ buffer_storage };

	char socket_buffer[size];
	asio::io_context ioc;

	using socket_type = es::test::mock_async_read_stream;
	using connection_type = es::test::mock_tcp_connection<socket_type>;

	socket_type read_stream{ ioc.get_executor(), socket_buffer, size };
	auto conn = std::make_shared<connection_type>(std::move(read_stream));

	const auto tcp_message_length = 200;

	std::size_t frame_size = 0;

	SECTION("a read operation executes two intermediate completion handlers")
	{
		// set tcp length prefixed frame to length = 200
		*reinterpret_cast<int*>(&socket_buffer[0]) = 200;

		std::error_code errc;
		es::tcp::operations::read_tcp_package_op<connection_type, dynamic_buffer_type> op{ conn, dynamic_buffer };
		op.initiate(
			[&frame_size, &errc](std::error_code ec, std::size_t message_size)
		{
			frame_size = message_size;
			errc = ec;
		}
		);

		auto completed_handler_count = ioc.poll_one();
		REQUIRE(completed_handler_count == 1);
		completed_handler_count += ioc.poll_one();
		REQUIRE(completed_handler_count == 2);
		// success error code
		REQUIRE(!errc);

		SECTION("the read operation's completion token is called with the whole tcp frame size if success")
		{
			REQUIRE(frame_size == tcp_message_length + 4);
		}
	}
	SECTION("a read operation executes two intermediate completion handlers")
	{
		// set tcp length prefixed frame to length = 17 (invalid)
		*reinterpret_cast<int*>(&socket_buffer[0]) = 17;

		std::error_code errc;
		es::tcp::operations::read_tcp_package_op<connection_type, dynamic_buffer_type> op{ conn, dynamic_buffer };
		op.initiate(
			[&frame_size, &errc](std::error_code ec, std::size_t message_size)
		{
			frame_size = message_size;
			errc = ec;
		}
		);

		auto completed_handler_count = ioc.poll_one();
		REQUIRE(completed_handler_count == 1);
		completed_handler_count += ioc.poll_one();
		// should not have called second completion handler
		REQUIRE(completed_handler_count == 1);
		REQUIRE(errc == es::communication_errors::bad_length_prefix);

		SECTION("the read operation's completion token is called with the whole tcp frame size if success")
		{
			// a frame length of 17 should cancel the read on the message, 
			// so we should have only read the length prefix (of 4 bytes, since it is an int)
			REQUIRE(frame_size == 4);
		}
	}
	SECTION("a read operation executes two intermediate completion handlers")
	{
		// set tcp length prefixed frame to length = max package size + 1 (invalid)
		*reinterpret_cast<int*>(&socket_buffer[0]) = es::detail::tcp::kMaxPackageSize + 1;

		std::error_code errc;
		es::tcp::operations::read_tcp_package_op<connection_type, dynamic_buffer_type> op{ conn, dynamic_buffer };
		op.initiate(
			[&frame_size, &errc](std::error_code ec, std::size_t message_size)
		{
			frame_size = message_size;
			errc = ec;
		}
		);

		auto completed_handler_count = ioc.poll_one();
		REQUIRE(completed_handler_count == 1);
		completed_handler_count += ioc.poll_one();
		// should not have called second completion handler
		REQUIRE(completed_handler_count == 1);
		REQUIRE(errc == es::communication_errors::bad_length_prefix);

		SECTION("the read operation's completion token is called with the whole tcp frame size if success")
		{
			// a frame length of 17 should cancel the read on the message, 
			// so we should have only read the length prefix (of 4 bytes, since it is an int)
			REQUIRE(frame_size == 4);
		}
	}
}