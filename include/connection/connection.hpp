#pragma once

#ifndef ES_CONNECTION_HPP
#define ES_CONNECTION_HPP

#include "basic_tcp_connection.hpp"
#include "tcp/basic_discovery_service.hpp"
#include "tcp/cluster_discovery_service.hpp"

namespace es {

using single_node_discovery_service = es::tcp::services::basic_discovery_service;
using cluster_discovery_service = es::tcp::services::cluster_discovery_service;

template <class DiscoveryService>
using tcp_connection = connection::basic_tcp_connection<
	boost::asio::ip::tcp::socket,
	boost::asio::steady_timer,
	DiscoveryService,
	es::operation<>
>;

using single_node_tcp_connection = connection::basic_tcp_connection<
	boost::asio::ip::tcp::socket,
	boost::asio::steady_timer,
	single_node_discovery_service,
	es::operation<>
>;

using cluster_node_tcp_connection = connection::basic_tcp_connection<
	boost::asio::ip::tcp::socket,
	boost::asio::steady_timer,
	cluster_discovery_service,
	es::operation<>
>;

}

#endif // ES_CONNECTION_HPP