#pragma once

#ifndef ES_BASIC_TCP_STREAM_HPP
#define ES_BASIC_TCP_STREAM_HPP

#include <deque>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/write.hpp>

#include "logger.hpp"
#include "guid.hpp"
#include "connection_result.hpp"
#include "connection_settings.hpp"
#include "duration_conversions.hpp"
#include "operations_map.hpp"

#include "buffer/dynamic_buffer.hpp"
#include "subscription/subscription_base.hpp"
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
	class DynamicBuffer = buffer::dynamic_buffer<std::uint8_t, Allocator>>
class basic_tcp_connection
	: public std::enable_shared_from_this<basic_tcp_connection<WaitableTimer, DiscoveryService, OperationType, Allocator, DynamicBuffer>>
{
public:
	using self_type = basic_tcp_connection;
	using executor_type = typename boost::asio::ip::tcp::socket::executor_type;
	using clock_type = typename WaitableTimer::clock_type;
	using operation_type = OperationType;
	using operations_map_type = es::operations_map<operation_type>;
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
		es::operations_map<operation_type>& get_operations_map(self_type& connection) { return connection.operations_map_; }
		es::operations_map<operation_type> const& get_operations_map(self_type& connection) const { return connection.operations_map_; }
		es::operations_map<operation_type>& get_subscriptions_map(self_type& connection) { return connection.subscriptions_map_; }
		es::operations_map<operation_type> const& get_subscriptios_map(self_type& connection) const { return connection.subscriptions_map_; }
		void async_start_receive(self_type& connection) { connection.async_start_receive(); }
	};
	template <class Friend>
	friend struct friend_base;

	template <class Friend>
	using friend_base_type = friend_base<Friend>;

	template <class T, class U>
	friend class subscription::subscription_base;

	explicit basic_tcp_connection(
		boost::asio::io_context& ioc,
		es::connection_settings const& settings,
		dynamic_buffer_type&& buffer = dynamic_buffer_type()
	) : socket_(ioc), 
		settings_(settings),
		connection_name_(std::string("ES-") + es::to_string(es::guid())),
		start_(clock_type::now()),
		message_queue_(),
		package_no_(0),
		operations_map_(),
		subscriptions_map_(),
		buffer_(std::move(buffer))
	{}

	template <class ConnectionResultHandler>
	void async_connect(ConnectionResultHandler&& handler)
	{
		static_assert(
			std::is_invocable_v<ConnectionResultHandler, boost::system::error_code, std::optional<connection_result>>,
			"ConnectionResultHandler requirements not met, must have signature R(boost::system::error_code, std::optional<connection_result>)"
		);

		auto identification_package_received_handler = 
			[handler = std::move(handler)](boost::system::error_code ec, detail::tcp::tcp_package_view view, guid_type connection_id = guid_type())
		{
			if (!ec || ec == es::connection_errors::authentication_failed)
			{
				// on success, command should be client identified
				if (view.command() != es::detail::tcp::tcp_command::client_identified) return;
				
				handler(ec, std::make_optional(connection_result{ connection_id }));
			}
			else
			{
				// es connection failed
				handler(ec, {});
			}
		};

		tcp::operations::connect_op<self_type, discovery_service_type, std::decay_t<decltype(identification_package_received_handler)>>
			op{ this->shared_from_this(), std::move(identification_package_received_handler) };

		op.initiate();
	}

	// send a tcp package to es server
	void async_send(detail::tcp::tcp_package<>&& package)
	{
		boost::asio::post(
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
			op{ this->shared_from_this(), std::move(handler), std::move(guid) };

		op.initiate();
	}

	// return socket
	boost::asio::ip::tcp::socket& socket() { return socket_; }
	// returns socket's executor
	executor_type get_executor() { return socket_.get_executor(); }
	// returns socket's io_context
	boost::asio::io_context& get_io_context() { return *reinterpret_cast<boost::asio::io_context*>(&socket_.get_executor().context()); }
	// returns elapsed time since the connection was created
	typename clock_type::duration elapsed() const { return clock_type::now() - start_; }
	// get connection settings
	es::connection_settings const& settings() const { return settings_; }
	// get connection name
	std::string const& connection_name() const { return connection_name_; }

	// close connection cleanly, this method needs help
	void close()
	{
		// do cleanup
		socket_.close();
	}
	
private:
	void on_package_received(boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		using tcp_command = detail::tcp::tcp_command;
		using tcp_flags = detail::tcp::tcp_flags;
		using tcp_package_view = detail::tcp::tcp_package_view;
		using tcp_package = detail::tcp::tcp_package<>;

		++package_no_;

		auto corr_id = es::guid(view.correlation_id().data());
		if (operations_map_.contains(corr_id))
		{
			operations_map_[corr_id](ec, view);
			operations_map_.erase(corr_id);
			return;
		}
		if (subscriptions_map_.contains(corr_id))
		{
			subscriptions_map_[corr_id](ec, view);
			// don't erase it, subscription operations should post their deletion to io_context
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
		tcp::operations::read_tcp_package_op<self_type, dynamic_buffer_type> read_op{ this->shared_from_this(), buffer_ };
		read_op.initiate(
			[this](boost::system::error_code& ec, std::size_t frame_size)
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
		boost::asio::async_write(
			socket_,
			boost::asio::buffer(message_queue_.front().data(), message_queue_.front().size()),
			[this](boost::system::error_code ec, std::size_t bytes_written)
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
	boost::asio::ip::tcp::socket socket_;
	es::connection_settings settings_;
	std::string connection_name_;
	std::chrono::time_point<clock_type> start_;
	std::deque<detail::tcp::tcp_package<>> message_queue_;
	unsigned int package_no_;

	operations_map_type operations_map_;
	operations_map_type subscriptions_map_;
	dynamic_buffer_type buffer_;
};

} // connection
} // es

#endif // ES_BASIC_TCP_STREAM_HPP