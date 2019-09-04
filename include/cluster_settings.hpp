#pragma once

#ifndef ES_CLUSTER_SETTINGS_HPP
#define ES_CLUSTER_SETTINGS_HPP

#include <string>
#include <chrono>

#include "tcp/gossip_seed.hpp"
#include "connection/node_preference.hpp"

namespace es {

class cluster_settings_builder;

class cluster_settings
{
public:
	friend class cluster_settings_builder;

	std::string const& cluster_dns() const { return cluster_dns_; }
	std::int32_t max_discover_attempts() const { return max_discover_attempts_; }
	std::int32_t external_gossip_port() const { return external_gossip_port_; }
	std::vector<tcp::gossip_seed> const& gossip_seeds() const { return gossip_seeds_; }
	std::chrono::nanoseconds gossip_timeout() const { return gossip_timeout_; }
	node_preference preference() const { return node_preference_; }

	// we need the default constructor for the cluster_discovery_service
	cluster_settings() = default;
private:

	std::string cluster_dns_;
	std::int32_t max_discover_attempts_;
	std::int32_t external_gossip_port_;
	std::vector<tcp::gossip_seed> gossip_seeds_;
	std::chrono::nanoseconds gossip_timeout_;
	node_preference node_preference_;
};

class cluster_settings_builder
{
public:
	using self_type = cluster_settings_builder;
	template <class GossipSeedOrEndpointIterator>
	self_type& with_gossip_seed_endpoints(GossipSeedOrEndpointIterator it, GossipSeedOrEndpointIterator end)
	{
		static_assert(std::is_same_v<std::decay_t<decltype(*it)>, tcp::gossip_seed> || 
			std::is_same_v<std::decay_t<decltype(*it)>, asio::ip::tcp::endpoint>, 
			"GossipSeedIterator must be dereferenceable to a tcp::gossip_seed& or to an asio::ip::tcp::endpoint&"
		);
		settings_.gossip_seeds_.clear();

		if constexpr (std::is_same_v<std::decay_t<decltype(*it)>, tcp::gossip_seed>)
		{
			settings_.gossip_seeds_.assign(it, end);
		}
		else
		{
			std::transform(it, end, 
				std::back_inserter(settings_.gossip_seeds_), 
				[](asio::ip::tcp::endpoint const& ep) { return tcp::gossip_seed(ep); });
		}
		return *this;
	}
	self_type& keep_discovering() { settings_.max_discover_attempts_ = std::numeric_limits<std::int32_t>::max(); return *this; }
	self_type& with_max_discover_attempts(std::int32_t attempts) { settings_.max_discover_attempts_ = attempts; return *this; }
	self_type& with_gossip_timeout(std::chrono::nanoseconds timeout) { settings_.gossip_timeout_ = timeout; return *this; }
	self_type& prefer_random_node() { settings_.node_preference_ = node_preference::random; return *this; }
	self_type& prefer_slave_node() { settings_.node_preference_ = node_preference::slave; return *this; }
	cluster_settings build() { return settings_; }

private:
	cluster_settings settings_;
};

}

#endif // ES_CLUSTER_SETTINGS_HPP