//#pragma once
//
//#ifndef ES_SECURE_TCP_CONNECTION_HPP
//#define ES_SECURE_TCP_CONNECTION_HPP
//
//#include "basic_tcp_connection.hpp"
//
//namespace es {
//namespace connection {
//
//template <
//	class WaitableTimer,
//	class DiscoveryService,
//	class OperationType,
//	class Allocator = std::allocator<std::uint8_t>,
//	class DynamicBuffer = buffer::dynamic_buffer<std::uint8_t, Allocator>>
//	class basic_tcp_connection
//	: public std::enable_shared_from_this<basic_tcp_connection<WaitableTimer, DiscoveryService, OperationType, Allocator, DynamicBuffer>>
//{
//public:
//	using self_type = basic_tcp_connection;
//	using executor_type = typename boost::asio::ip::tcp::socket::executor_type;
//	using clock_type = typename WaitableTimer::clock_type;
//	using operation_type = OperationType;
//	using operations_map_type = es::operations_map<operation_type>;
//	using discovery_service_type = DiscoveryService;
//	using allocator_type = Allocator;
//	using dynamic_buffer_type = DynamicBuffer;
//	using waitable_timer_type = WaitableTimer;
//
//	// make type checks here for template arguments
//	// static_assert(... "...");
//
//	// make it so that every operation
//	// can access connection's members
//	template <class Friend>
//	struct friend_base
//	{
//		unsigned int get_package_number(self_type& connection) const { return connection.package_number(); }
//		es::operations_map<operation_type>& get_operations_map(self_type& connection) { return connection.operations_map_; }
//		es::operations_map<operation_type> const& get_operations_map(self_type& connection) const { return connection.operations_map_; }
//		es::operations_map<operation_type>& get_subscriptions_map(self_type& connection) { return connection.subscriptions_map_; }
//		es::operations_map<operation_type> const& get_subscriptios_map(self_type& connection) const { return connection.subscriptions_map_; }
//		void async_start_receive(self_type& connection) { connection.async_start_receive(); }
//	};
//	template <class Friend>
//	friend struct friend_base;
//
//	template <class Friend>
//	using friend_base_type = friend_base<Friend>;
//
//	template <class T, class U>
//	friend class subscription::subscription_base;
//
//	explicit basic_tcp_connection(
//		boost::asio::io_context& ioc,
//		es::connection_settings const& settings,
//		dynamic_buffer_type&& buffer = dynamic_buffer_type(),
//		std::function<std::string()> conn_name_generator = []() { return std::string("ES-") + es::to_string(es::guid()); }
//	) : socket_(ioc),
//		settings_(settings),
//		start_(clock_type::now()),
//		message_queue_(),
//		package_no_(0),
//		operations_map_(),
//		subscriptions_map_(),
//		buffer_(std::move(buffer)),
//		is_closed_(true),
//		connection_name_generator_(conn_name_generator),
//		connection_name_()
//	{}
//
//	template <class ConnectionResultHandler>
//	void async_connect(ConnectionResultHandler&& handler)
//	{
//		static_assert(
//			std::is_invocable_v<ConnectionResultHandler, boost::system::error_code, std::optional<connection_result>>,
//			"ConnectionResultHandler requirements not met, must have signature R(boost::system::error_code, std::optional<connection_result>)"
//			);
//
//		// generate new connection name on every connect call
//		connection_name_ = connection_name_generator_();
//
//		auto identification_package_received_handler =
//			[this, handler = std::move(handler)](boost::system::error_code ec, detail::tcp::tcp_package_view view, guid_type connection_id = guid_type())
//		{
//			if (!ec || ec == es::connection_errors::authentication_failed)
//			{
//				// on success, command should be client identified
//				if (view.command() != es::detail::tcp::tcp_command::client_identified) return;
//
//				is_closed_ = false;
//				handler(ec, std::make_optional(connection_result{ connection_id }));
//			}
//			else
//			{
//				is_closed_ = false;
//				this->close([]() {});
//				// es connection failed
//				handler(ec, {});
//			}
//		};
//
//		tcp::operations::connect_op<self_type, discovery_service_type, std::decay_t<decltype(identification_package_received_handler)>>
//			op{ this->shared_from_this(), std::move(identification_package_received_handler) };
//
//		op.initiate();
//	}
//
//	// send a tcp package to es server
//	void async_send(detail::tcp::tcp_package<>&& package)
//	{
//		boost::asio::post(
//			get_io_context(),
//			// use shared from this? it would extend the lifetime of the connection, even if client does not have any more references to it...
//			[this, package = std::move(package)]() mutable
//		{
//			bool write_in_progress = !this->message_queue_.empty();
//			message_queue_.emplace_back(std::move(package));
//			if (!write_in_progress)
//			{
//				do_async_send();
//			}
//		});
//	}
//
//	// send tcp package with notification
//	template <class PackageReceivedHandler>
//	void async_send(detail::tcp::tcp_package<>&& package, PackageReceivedHandler&& handler)
//	{
//		auto view = static_cast<detail::tcp::tcp_package_view>(package);
//		auto guid = es::guid(view.correlation_id().data());
//
//		// put package in message queue before initiating the operation (doesn't actually matter, just to give the timeout wait an extra nanosecond, haha)
//		this->async_send(std::move(package));
//
//		tcp::operations::operation_op<self_type, waitable_timer_type, PackageReceivedHandler>
//			op{ this->shared_from_this(), std::move(handler), std::move(guid) };
//
//		op.initiate();
//	}
//
//	// return socket
//	boost::asio::ip::tcp::socket& socket() { return socket_; }
//	// returns socket's executor
//	executor_type get_executor() { return socket_.get_executor(); }
//	// returns socket's io_context
//	boost::asio::io_context& get_io_context() { return *reinterpret_cast<boost::asio::io_context*>(&socket_.get_executor().context()); }
//	// returns elapsed time since the connection was created
//	typename clock_type::duration elapsed() const { return clock_type::now() - start_; }
//	// get connection settings
//	es::connection_settings const& settings() const { return settings_; }
//	// get connection name
//	std::string const& connection_name() const { return connection_name_; }
//
//	bool is_closed() const { return is_closed_; }
//
//	// close connection
//	template <class ConnectionClosedHandler>
//	void close(ConnectionClosedHandler&& handler)
//	{
//		if (is_closed_) return;
//
//		// do cleanup
//		is_closed_ = true;
//
//		boost::system::error_code ec;
//		socket_.shutdown(socket_.shutdown_both, ec);
//		boost::asio::post(
//			this->get_executor(),
//			[this, handler = std::forward<ConnectionClosedHandler>(handler)]()
//		{
//			boost::system::error_code ec;
//			// use error code so that even if socket is closed, the call doesn't throw
//			socket_.close(ec);
//			handler();
//		});
//	}
//
//	void close()
//	{
//		this->close([]() {});
//	}
//
//private:
//	boost::asio::ip::tcp::socket socket_;
//};
//
//} // connection
//} // es
//
//#endif // ES_SECURE_TCP_CONNECTION_HPP