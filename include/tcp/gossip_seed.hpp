#pragma once

#ifndef GOSSIP_SEED_HPP
#define GOSSIP_SEED_HPP

#include <string>

#include <boost/asio/ip/tcp.hpp>

namespace es {
namespace tcp {

class gossip_seed
{
public:
	explicit gossip_seed(
		boost::asio::ip::tcp::endpoint const& endpoint,
		std::string const& host_header = ""
	)
		: endpoint_(endpoint), host_header_(host_header)
	{}

	boost::asio::ip::tcp::endpoint const& endpoint() const { return endpoint_; }
	std::string const& host_header() const { return host_header_; }

private:
	boost::asio::ip::tcp::endpoint endpoint_;
	std::string host_header_;
};

} // tcp
} // es

#endif // GOSSIP_SEED_HPP