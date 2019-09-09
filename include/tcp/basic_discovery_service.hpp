#pragma once

#ifndef ES_BASIC_DISCOVERY_SERVICE_HPP
#define ES_BASIC_DISCOVERY_SERVICE_HPP

#include <type_traits>

#include <boost/asio/io_context.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "node_endpoints.hpp"

namespace es {
namespace tcp {
namespace services {

class basic_discovery_service
	: public boost::asio::execution_context::service
{
public:
	using endpoint_type = boost::asio::ip::tcp::endpoint;
	using node_endpoints_type = node_endpoints;
	using key_type = basic_discovery_service;
	inline static boost::asio::execution_context::id id;

	explicit basic_discovery_service(
		boost::asio::execution_context& ioc
	) : boost::asio::execution_context::service(ioc),
		endpoint_(),
		secure_endpoint_(),
		ssl_(),
		master_()
	{}

	explicit basic_discovery_service(
		boost::asio::execution_context& ioc,
		boost::asio::ip::tcp::endpoint const& endpoint,
		boost::asio::ip::tcp::endpoint const& secure_endpoint,
		bool ssl
	)
		: boost::asio::execution_context::service(ioc),
		endpoint_(endpoint),
		secure_endpoint_(secure_endpoint),
		ssl_(ssl),
		master_()
	{}

	template <class Connection, class Func>
	void async_discover_node_endpoints(Connection& connection, Func&& f)
	{
		static_assert(
			std::is_invocable_v<Func, boost::system::error_code, endpoint_type>,
			"template argument to Func must have signature void(boost::system::error_code, endpoint_type)"
			);

		boost::asio::post(connection.get_io_context(),
			[this, f = std::move(f)]() mutable
		{
			boost::system::error_code ec;
			f(ec, ssl_ ? this->node_endpoint_secure() : this->node_endpoint());
		});
	}

	endpoint_type& node_endpoint() { return endpoint_; }
	endpoint_type& node_endpoint_secure() { return secure_endpoint_; }
	endpoint_type const& node_endpoint() const { return endpoint_; }
	endpoint_type const& node_endpoint_secure() const { return secure_endpoint_; }
	std::optional<node_endpoints_type> const& master() const { return master_; }
	void set_master(node_endpoints_type const& eps) 
	{ 
		endpoint_ = eps.tcp_endpoint();
		secure_endpoint_ = eps.secure_tcp_endpoint();
	}

	~basic_discovery_service()
	{
		shutdown();
	}
private:
	virtual void shutdown() noexcept override
	{}

	endpoint_type endpoint_;
	endpoint_type secure_endpoint_;
	bool ssl_;
	std::optional<node_endpoints_type> master_;
};

} // services
} // tcp
} // es

#endif // ES_BASIC_DISCOVERY_SERVICE_HPP