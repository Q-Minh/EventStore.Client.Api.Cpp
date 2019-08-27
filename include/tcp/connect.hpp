#pragma once

#ifndef ES_CONNECT_HPP
#define ES_CONNECT_HPP

#include <memory>
#include <functional>

#include <asio/execution_context.hpp>

#include "logger.hpp"

#include "error/error.hpp"
#include "tcp/tcp_commands.hpp"
#include "tcp/tcp_flags.hpp"
#include "tcp/tcp_package.hpp"
#include "tcp/authenticate.hpp"
#include "tcp/identify.hpp"
#include "tcp/heartbeat.hpp"
#include "message/messages.pb.h"

namespace es {
namespace tcp {
namespace operations {

template <class ConnectionType, class PackageReceivedHandler> class authentication_handler;

template <class ConnectionType, class PackageReceivedHandler>
class tcp_connected_handler
	: public ConnectionType::template friend_base_type<tcp_connected_handler<ConnectionType, PackageReceivedHandler>>
{
public:
	using connection_type = ConnectionType;
	using handler_type = PackageReceivedHandler;

	explicit tcp_connected_handler(
		std::shared_ptr<connection_type> const& connection,
		PackageReceivedHandler&& handler
	) : connection_(connection), handler_(std::move(handler))
	{}

	void operator()(asio::error_code ec)
	{
		// please set error code...
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		if (!ec)
		{
			auto& settings = conn->settings();
			if (settings.default_user_credentials().null())
			{
				ES_DEBUG("tcp_connected_handler::operator() : no default user credentials");
				// send tcp package with IdentifyClient message
				identify_op<connection_type, handler_type> op{ conn, std::move(handler_) };
				op.initiate();
				ES_DEBUG("tcp_connected_handler::operator() : initiated identify operation");
			}
			else
			{
				ES_DEBUG("tcp_connected_handler::operator() : default user credentials were specified");

				authentication_handler<connection_type, handler_type> handler(conn, std::move(handler_));

				// send tcp package with tcp Authenticate command, tcp authenticated flags
				authenticate_op<connection_type, decltype(handler)> op{ conn, std::move(handler) };
				op.initiate();
				ES_DEBUG("tcp_connected_handler::operator() : initiated authentication operation");
			}
			
			ES_DEBUG("tcp_connected_handler::operator() : start listening for tcp packages from es server");
			this->async_start_receive(*conn);
			heartbeat_op<connection_type> op{ conn };
			op.initiate();
		}
		else
		{
			ES_ERROR("tcp_connected_handler::operator() : tcp connection failed, {}", ec.message());
			handler_(ec, {});
			return;
		}
	}
private:
	std::weak_ptr<connection_type> connection_;
	PackageReceivedHandler handler_;
};

template <class ConnectionType, class PackageReceivedHandler>
class authentication_handler
{
public:
	explicit authentication_handler(
		std::shared_ptr<ConnectionType> const& connection,
		PackageReceivedHandler&& handler
	) : connection_(connection), handler_(handler)
	{}

	authentication_handler(authentication_handler<ConnectionType, PackageReceivedHandler>&& other) = default;

	void operator()(asio::error_code ec, detail::tcp::tcp_package_view view)
	{
		// signal error
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		bool authenticated = false;
		if (ec)
		{
			// failed to authenticate, notify users
			ES_WARN("tcp_connected_handler::operator()::intermediate_handler : authentication failed, {}", ec.message());
		}
		else
		{
			if (view.command() == detail::tcp::tcp_command::authenticated)
			{
				ES_INFO("tcp_connected_handler::operator()::intermediate_handler : authentication succeeded");
				authenticated = true;
			}
			else if (view.command() == detail::tcp::tcp_command::not_authenticated)
			{
				ES_INFO("tcp_connected_handler::operator()::intermediate_handler : authentication refused");
			}
			else
			{
				ES_ERROR("tcp_connected_handler::operator()::intermediate_handler : bug! sent authentication request, but got garbage answer");
				return;
			}
		}

		// can still identify to es
		// send tcp package with IdentifyClient message
		identify_op<ConnectionType, PackageReceivedHandler> op(conn, std::move(handler_), authenticated);
		op.initiate();
		ES_DEBUG("tcp_connected_handler::operator() : initiated identify operation with {}",
			authenticated ? "authentication" : "no authentication"
		);
	}
private:
	std::weak_ptr<ConnectionType> connection_;
	PackageReceivedHandler handler_;
};

template <class ConnectionType, class DiscoveryService, class PackageReceivedHandler>
class tcp_connect_op
{
public:
	using connection_type = ConnectionType;
	using discovery_service_type = DiscoveryService;
	using handler_type = PackageReceivedHandler;
	
	explicit tcp_connect_op(
		std::shared_ptr<connection_type> const& connection,
		PackageReceivedHandler&& handler,
		typename DiscoveryService::endpoint_type const& endpoint
	)
		: connection_(connection), handler_(std::move(handler)), endpoint_(endpoint)
	{}

	void initiate()
	{
		// should give error code
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		tcp_connected_handler<connection_type, handler_type> handler{ conn, std::move(handler_) };
		conn->socket().async_connect(
			endpoint_,
			std::move(handler)
		);

		std::ostringstream oss{};
		oss << endpoint_;
		ES_DEBUG("tcp_connect_op::initiate : initiated socket connection to {}", oss.str());
	}

private:
	std::weak_ptr<connection_type> connection_;
	PackageReceivedHandler handler_;
	typename DiscoveryService::endpoint_type endpoint_;
};

template <class ConnectionType, class DiscoveryService, class PackageReceivedHandler>
class discover_node_endpoints_handler
{
public:
	using connection_type = ConnectionType;
	using discovery_service_type = DiscoveryService;
	using handler_type = PackageReceivedHandler;
	using endpoint_type = typename DiscoveryService::endpoint_type;

	explicit discover_node_endpoints_handler(
		std::shared_ptr<connection_type> const& connection,
		PackageReceivedHandler&& handler
	) : connection_(connection), handler_(std::move(handler))
	{}

	void operator()(asio::error_code ec, endpoint_type endpoint)
	{
		// set error code....
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		if (!ec)
		{
			tcp_connect_op<connection_type, discovery_service_type, handler_type>
				connect_op{ conn, std::move(handler_), endpoint };
			connect_op.initiate();
			ES_DEBUG("discover_node_endpoints_handler::operator() : initiated tcp connect operation");
		}
		else
		{
			ES_ERROR("discover_node_endpoints_handler::operator() : discover node endpoints failed, {}", ec.message());
			ec = make_error_code(connection_errors::endpoint_discovery);
			handler_(ec, {});
		}
	}

private:
	std::weak_ptr<connection_type> connection_;
	PackageReceivedHandler handler_;
};

template <class ConnectionType, class DiscoveryService, class PackageReceivedHandler>
class connect_op
	: public ConnectionType::template friend_base_type<connect_op<ConnectionType, DiscoveryService, PackageReceivedHandler>>
{
public:
	using connection_type = ConnectionType;
	using discovery_service_type = DiscoveryService;
	using handler_type = PackageReceivedHandler;

	explicit connect_op(
		std::shared_ptr<connection_type> const& connection,
		PackageReceivedHandler&& handler
	) : connection_(connection), handler_(std::move(handler))
	{}

	void initiate()
	{
		// we should set more error codes ...
		if (connection_.expired()) return;
		auto conn = connection_.lock();

		auto& endpoint_discoverer = asio::use_service<discovery_service_type>(conn->get_io_context());

		discover_node_endpoints_handler<connection_type, discovery_service_type, handler_type> handler{ conn, std::move(handler_) };
		endpoint_discoverer.async_discover_node_endpoints(std::move(handler));
		ES_DEBUG("connect_op::initiate : initiated endpoints discovery");
	}

private:
	std::weak_ptr<connection_type> connection_;
	PackageReceivedHandler handler_;
};

} // operations
} // tcp
} // es

#endif // ES_CONNECT_HPP