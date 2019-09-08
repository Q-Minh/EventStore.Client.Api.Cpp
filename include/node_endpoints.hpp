#pragma once

#ifndef ES_NODE_ENDPOINTS_HPP
#define ES_NODE_ENDPOINTS_HPP

#include <boost/asio/ip/tcp.hpp>

namespace es {

class node_endpoints
{
public:
	using endpoint_type = boost::asio::ip::tcp::endpoint;

	node_endpoints(
		endpoint_type const& tcp_endpoint,
		endpoint_type const& secure_tcp_endpoint
	)
		: endpoint_(tcp_endpoint), secure_endpoint_(secure_tcp_endpoint)
	{}

	endpoint_type const& tcp_endpoint() const { return endpoint_; }
	endpoint_type const& secure_tcp_endpoint() const { return secure_endpoint_; }
private:
	endpoint_type endpoint_;
	endpoint_type secure_endpoint_;
};

}

#endif // ES_NODE_ENDPOINTS_HPP