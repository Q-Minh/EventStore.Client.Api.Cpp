#pragma once

#ifndef ES_BASIC_TCP_STREAM_HPP
#define ES_BASIC_TCP_STREAM_HPP

#include <deque>
#include <memory>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/error_code.hpp>
#include <asio/write.hpp>
#include <asio/buffer.hpp>

#include "logger.hpp"
#include "guid.hpp"
#include "connection_settings.hpp"
#include "duration_conversions.hpp"
#include "operations_manager.hpp"

#include "tcp/tcp_package.hpp"
#include "tcp/read.hpp"
#include "tcp/connect.hpp"
#include "tcp/operation.hpp"

namespace es {
namespace connection {

// sorry for all the template parameters...
template <
	class WaitableTimer, 
	class DiscoveryService, 
	class OperationType, 
	class Allocator = std::allocator<std::uint8_t>,
	class DynamicBuffer = asio::dynamic_vector_buffer<std::uint8_t, Allocator>>
class basic_tcp_connection
	: public std::enable_shared_from_this<basic_tcp_connection<WaitableTimer, DiscoveryService, OperationType, Allocator, DynamicBuffer>>
{
public:
	using self_type = basic_tcp_connection;
	using executor_type = typename asio::ip::tcp::socket::executor_type;
	using clock_type = typename WaitableTimer::clock_type;
	using operation_type = OperationType;
	using operations_manager_type = es::operations_manager<operation_type>;
	using discovery_service_type = DiscoveryService;
	using allocator_type = Allocator;
	using dynamic_buffer_type = DynamicBuffer;
	using waitable_timer_type = WaitableTimer;

	// make type checks here for template arguments
	// static_assert(... "...");

	// make it so that every operation
	// can access connection's members
	template <class Friend>
	struct friend_base 
	{
		unsigned int get_package_number(self_type& connection) const { return connection.package_number(); }
		es::operations_manager<operation_type>& get_operations_manager(self_type& connection) { return connection.operations_manager_; }
		es::operations_manager<operation_type> const& get_operations_manager(self_type& connection) const { return connection.operations_manager_; }
		void async_start_receive(self_type& connection) { connection.async_start_receive(); }
	};
	template <class Friend>
	friend struct friend_base;

	template <class Friend>
	using friend_base_type = friend_base<Friend>;

	explicit basic_tcp_connection(
		asio::io_context& ioc,
		es::connection_settings const& settings,
		dynamic_buffer_type&& buffer
	) : socket_(ioc), 
		settings_(settings),
		connection_name_(std::string("ES-") + es::to_string(es::guid())),
		start_(clock_type::now()),
		message_queue_(),
		package_no_(0),
		operations_manager_(),
		buffer_(std::move(buffer))
	{}

	template <class PackageReceivedHandler>
	void async_connect(PackageReceivedHandler&& handler)
	{
		tcp::operations::connect_op<self_type, discovery_service_type, PackageReceivedHandler>
			op{ shared_from_this(), std::move(handler) };

		op.initiate();
	}

	// send a tcp package to es server
	void async_send(detail::tcp::tcp_package<>&& package)
	{
		asio::post(
			get_io_context(),
			// use shared from this? it would extend the lifetime of the connection, even if client does not have any more references to it...
			[this, package = std::move(package)]() mutable
		{
			bool write_in_progress = !this->message_queue_.empty();
			message_queue_.emplace_back(std::move(package));
			if (!write_in_progress)
			{
				do_async_send();
			}
		}
		);
	}

	// send tcp package with notification
	template <class PackageReceivedHandler>
	void async_send(detail::tcp::tcp_package<>&& package, PackageReceivedHandler&& handler)
	{
		auto view = static_cast<detail::tcp::tcp_package_view>(package);
		auto guid = es::guid(view.correlation_id().data());

		// put package in message queue before initiating the operation (doesn't actually matter, just to give the timeout wait an extra nanosecond, haha)
		this->async_send(std::move(package));

		tcp::operations::operation_op<self_type, waitable_timer_type, PackageReceivedHandler>
			op{ shared_from_this(), std::move(handler), std::move(guid) };

		op.initiate();
	}

	// return socket
	asio::ip::tcp::socket& socket() { return socket_; }
	// returns socket's executor
	executor_type get_executor() { return socket_.get_executor(); }
	// returns socket's io_context
	asio::io_context& get_io_context() { return socket_.get_io_context(); }
	// returns elapsed time since the connection was created
	typename clock_type::duration elapsed() const { return clock_type::now() - start_; }
	// get connection settings
	es::connection_settings const& settings() const { return settings_; }
	// get connection name
	std::string const& connection_name() const { return connection_name_; }

	// close connection cleanly
	void close()
	{
		// do cleanup
		socket_.close();
	}
	
private:
	void on_package_received(asio::error_code ec, detail::tcp::tcp_package_view view)
	{
		using tcp_command = detail::tcp::tcp_command;
		using tcp_flags = detail::tcp::tcp_flags;
		using tcp_package_view = detail::tcp::tcp_package_view;
		using tcp_package = detail::tcp::tcp_package<>;

		++package_no_;

		auto corr_id = es::guid(view.correlation_id().data());
		if (operations_manager_.contains(corr_id))
		{
			operations_manager_[corr_id](ec, view);
			operations_manager_.erase(corr_id);
			return;
		}

		switch (view.command())
		{
		case tcp_command::heartbeat_request_command:
			ES_TRACE("basic_tcp_connection::on_package_received : got heartbeat request, replying");
			async_send(tcp_package(
				tcp_command::heartbeat_response_command,
				tcp_flags::none,
				es::guid()
			));
			break;
		case tcp_command::heartbeat_response_command:
			ES_TRACE("basic_tcp_connection::on_package_received : got heartbeat response");
			break;
		case tcp_command::bad_request:
			ES_ERROR("basic_tcp_connection::on_package_received : got bad request reply from server");
			// do something
			break;
		default:
			ES_WARN("basic_tcp_connection::on_package_received : received unexpected server response");
			return;
		}
	}

	void async_start_receive()
	{
		tcp::operations::read_tcp_package_op<self_type, dynamic_buffer_type> read_op{ shared_from_this(), buffer_ };
		read_op.initiate(
			[this](asio::error_code& ec, std::size_t frame_size)
		{
			if (!ec)
			{
				detail::tcp::tcp_package_view view(
					(std::byte*)buffer_.data().data(),
					frame_size /*or buffer.size()*/
				);

				ES_TRACE("got package : cmd={}, msg-offset={}, msg-size={}, msg={}",
					(unsigned int)view.command(),
					view.message_offset(),
					view.message_size(),
					std::string_view(view.data() + view.message_offset(), view.message_size())
				);

				on_package_received(ec, view);

				this->async_start_receive();
			}
			else
			{
				close();
			}
		}
		);
	}

	void do_async_send()
	{
		asio::async_write(
			socket_,
			asio::buffer(message_queue_.front().data(), message_queue_.front().size()),
			[this](asio::error_code ec, std::size_t bytes_written)
		{
			if (!ec)
			{
				message_queue_.pop_front();
				if (!message_queue_.empty()) do_async_send();
			}
			else
			{
				// report error
				socket_.close();
			}
		}
		);
	}
	
	unsigned int& package_number() { return package_no_; }
	unsigned int const& package_number() const { return package_no_; }

private:
	asio::ip::tcp::socket socket_;
	es::connection_settings settings_;
	std::string connection_name_;
	std::chrono::time_point<clock_type> start_;
	std::deque<detail::tcp::tcp_package<>> message_queue_;
	unsigned int package_no_;

	operations_manager_type operations_manager_;
	dynamic_buffer_type buffer_;
};

} // connection
} // es

#endif // ES_BASIC_TCP_STREAM_HPP