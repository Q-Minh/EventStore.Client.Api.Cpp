#pragma once

#ifndef ES_DISCOVERY_SERVICE_HPP
#define ES_DISCOVERY_SERVICE_HPP

#include <type_traits>

#include <asio/io_context.hpp>
#include <asio/execution_context.hpp>
#include <asio/ip/tcp.hpp>

namespace es {
namespace tcp {
namespace services {

class discovery_service
	: public asio::execution_context::service
{
public:
	using endpoint_type = asio::ip::tcp::endpoint;
	using key_type = discovery_service;
	inline static asio::execution_context::id id;

	explicit discovery_service(
		asio::execution_context& ioc
	) : asio::execution_context::service(ioc),
		endpoint_(),
		secure_endpoint_(),
		ssl_()
	{}

	explicit discovery_service(
		asio::execution_context& ioc,
		asio::ip::tcp::endpoint const& endpoint,
		asio::ip::tcp::endpoint const& secure_endpoint,
		bool ssl
	)
		: asio::execution_context::service(ioc),
		endpoint_(endpoint),
		secure_endpoint_(secure_endpoint),
		ssl_(ssl)
	{}

	template <class Func>
	void async_discover_node_endpoints(Func&& f)
	{
		static_assert(
			std::is_invocable_v<Func, asio::error_code, endpoint_type>,
			"template argument to Func must have signature void(asio::error_code, endpoint_type)"
			);

		asio::post(reinterpret_cast<asio::io_context&>(this->context()),
			[this, f = std::move(f)]() mutable
		{
			asio::error_code ec;
			f(ec, ssl_ ? this->node_endpoint_secure() : this->node_endpoint());
		});
	}

	/*endpoint_type& node_endpoint() { return endpoint_; }
	endpoint_type& node_endpoint_secure() { return secure_endpoint_; }*/
	endpoint_type const& node_endpoint() const { return endpoint_; }
	endpoint_type const& node_endpoint_secure() const { return secure_endpoint_; }

	~discovery_service()
	{
		shutdown();
	}
private:
	virtual void shutdown() noexcept override
	{}

	endpoint_type endpoint_;
	endpoint_type secure_endpoint_;
	bool ssl_;
};

} // services
} // tcp
} // es

#endif // ES_DISCOVERY_SERVICE_HPP