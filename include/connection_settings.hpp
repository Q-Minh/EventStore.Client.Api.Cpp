#pragma once

#ifndef CONNECTION_SETTINGS_HPP
#define CONNECTION_SETTINGS_HPP

#include <string>
#include <vector>
#include <chrono>

#include "connection/constants.hpp"
#include "connection/node_preference.hpp"
#include "tcp/gossip_seed.hpp"
#include "user/user_credentials.hpp"

namespace es {

class connection_settings_builder;

class connection_settings
{
public:
	static connection_settings null() { return connection_settings(); }

    connection_settings(connection_settings const& other) = default;
    connection_settings& operator=(connection_settings const&) = delete;

    //std::uint32_t max_queue_size() const { return max_queue_size_; }
    std::uint32_t max_concurrent_items() const { return max_concurrent_items_; }
    //std::uint32_t max_retries() const { return max_retries_; }
    //std::uint32_t max_reconnections() const { return max_reconnections_; }
    bool require_master() const { return require_master_; }
    //std::chrono::milliseconds reconnection_delay() const { return reconnection_delay_; }
    //std::chrono::milliseconds queue_timeout() const { return queue_timeout_; }
    std::chrono::seconds operation_timeout() const { return operation_timeout_; }
    //std::chrono::seconds operation_timeout_check_period() const { return operation_timeout_check_period_; }
    std::chrono::milliseconds heartbeat_interval() const { return heartbeat_interval_; }
    std::chrono::milliseconds heartbeat_timeout() const { return heartbeat_timeout_; }
    //std::chrono::milliseconds client_connection_timeout() const { return client_connection_timeout_; }
    //std::uint32_t max_discover_attempts() const { return max_discover_attempts_; }
    //std::uint32_t gossip_external_http_port() const { return gossip_external_http_port_; }
    //std::chrono::seconds gossip_timeout() const { return gossip_timeout_; }
    //node_preference preference() const { return node_preference_; }
    user::user_credentials const& default_user_credentials() const { return default_user_credentials_; }
    bool use_ssl() const { return use_ssl_; }
    std::string const& target_host() const { return target_host_; }
    bool validate_server() const { return validate_server_; }
    bool fail_on_no_server_response() const { return fail_on_no_server_response_; }
    //std::string const& cluster_dns() const { return cluster_dns_; }
    std::vector<tcp::gossip_seed> const& gossip_seeds() const { return gossip_seeds_; }

private:
    explicit connection_settings() = default;
    friend class connection_settings_builder;

    bool verbose_;
    std::uint32_t max_queue_size_ = es::connection::constants::kDefaultMaxQueueSize;
    std::uint32_t max_concurrent_items_ = es::connection::constants::kDefaultMaxConcurrentItems;
    std::uint32_t max_retries_ = es::connection::constants::kDefaultMaxOperationRetries;
    std::uint32_t max_reconnections_ = es::connection::constants::kDefaultMaxReconnections;
    bool require_master_ = es::connection::constants::kDefaultRequireMaster;
    std::chrono::milliseconds reconnection_delay_ = es::connection::constants::kDefaultReconnectionDelay;
    std::chrono::milliseconds queue_timeout_ = es::connection::constants::kDefaultQueueTimeout;
    std::chrono::seconds operation_timeout_ = es::connection::constants::kDefaultOperationTimeout;
    std::chrono::seconds operation_timeout_check_period_ = es::connection::constants::kDefaultOperationTimeoutCheckPeriod;
    std::chrono::milliseconds heartbeat_interval_ = es::connection::constants::kDefaultHeartbeatInterval;
    std::chrono::milliseconds heartbeat_timeout_ = es::connection::constants::kDefaultHeartbeatTimeout;
    std::chrono::milliseconds client_connection_timeout_ = es::connection::constants::kDefaultClientConnectionTimeout;
    std::uint32_t max_discover_attempts_ = es::connection::constants::kDefaultMaxClusterDiscoverAttempts;
    std::uint32_t gossip_external_http_port_ = es::connection::constants::kDefaultClusterManagerExternalHttpPort;
    std::chrono::seconds gossip_timeout_ = es::connection::constants::kDefaultGossipTimeout;

    node_preference node_preference_ = node_preference::master;

    user::user_credentials default_user_credentials_;
    bool use_ssl_;
    std::string target_host_;
    bool validate_server_;
    bool fail_on_no_server_response_;
    std::string cluster_dns_;

    std::vector<tcp::gossip_seed> gossip_seeds_; 
};

class connection_settings_builder
{
private:
    using self_type = connection_settings_builder;
    connection_settings settings_;
public:
    explicit connection_settings_builder() = default;

    self_type& with_max_queue_size(std::uint32_t max_queue_size) { settings_.max_queue_size_ = max_queue_size; return *this; }
    self_type& with_max_concurrent_items(std::uint32_t max_concurrent_items) { settings_.max_concurrent_items_ = max_concurrent_items; return *this; }
    self_type& with_max_retries(std::uint32_t max_retries) { settings_.max_retries_ = max_retries; return *this; }
    self_type& with_max_reconnections(std::uint32_t max_reconnections) { settings_.max_reconnections_ = max_reconnections; return *this; }
    self_type& require_master(bool require) { settings_.require_master_ = require; return *this; }
    self_type& with_reconnection_delay(std::chrono::milliseconds reconnection_delay) { settings_.reconnection_delay_ = reconnection_delay; return *this; }
    self_type& with_queue_timeout(std::chrono::milliseconds queue_timeout) { settings_.queue_timeout_ = queue_timeout; return *this; }
    self_type& with_operation_timeout(std::chrono::seconds operation_timeout) { settings_.operation_timeout_ = operation_timeout; return *this; }
    self_type& with_operation_timeout_check_period(std::chrono::seconds operation_timeout_check_period) { settings_.operation_timeout_check_period_ = operation_timeout_check_period; return *this; }
    self_type& with_heartbeat_interval(std::chrono::milliseconds heartbeat_interval) { settings_.heartbeat_interval_ = heartbeat_interval; return *this; }
    self_type& with_heartbeat_timeout(std::chrono::milliseconds heartbeat_timeout) { settings_.heartbeat_timeout_ = heartbeat_timeout; return *this; }
    self_type& with_client_connection_timeout(std::chrono::milliseconds client_connection_timeout) { settings_.client_connection_timeout_ = client_connection_timeout; return *this; }
    self_type& with_max_discover_attempts(std::uint32_t max_discover_attempts) { settings_.max_discover_attempts_ = max_discover_attempts; return *this; }
    self_type& with_gossip_external_http_port(std::uint32_t port) { settings_.gossip_external_http_port_ = port; return *this; }
    self_type& with_gossip_timeout(std::chrono::seconds gossip_timeout) { settings_.gossip_timeout_ = gossip_timeout; return *this; }
    self_type& prefer_random_node() { settings_.node_preference_ = node_preference::random; return *this; }
	self_type& prefer_slave_node() { settings_.node_preference_ = node_preference::slave; return *this; }
    self_type& with_default_user_credentials(user::user_credentials const& creds) { settings_.default_user_credentials_ = creds; return *this; }
    self_type& use_ssl(bool use) { settings_.use_ssl_ = use; return *this; }
    self_type& with_target_host(std::string const& target_host) { settings_.target_host_ = target_host; return *this; }
    self_type& validate_server(bool validate) { settings_.validate_server_ = validate; return *this; }
    self_type& fail_on_no_server_response(bool fail) { settings_.fail_on_no_server_response_ = fail; return *this; }
    self_type& with_cluster_dns(std::string const& dns) { settings_.cluster_dns_ = dns; return *this; }
    self_type& with_gossip_seeds(std::vector<tcp::gossip_seed> const& seeds)
    {
        settings_.gossip_seeds_.clear();
        settings_.gossip_seeds_.insert(settings_.gossip_seeds_.end(), seeds.cbegin(), seeds.cend());
        return *this;
    }
	self_type& with_gossip_seeds(std::vector<boost::asio::ip::tcp::endpoint> const& seeds)
	{
		settings_.gossip_seeds_.clear();
		for (auto const& endpoint : seeds)
		{
			settings_.gossip_seeds_.push_back(tcp::gossip_seed(endpoint));
		}
        return *this;
	}

    connection_settings build()
    {
        return settings_;
    }
};

} // es

#endif // CONNECTION_SETTINGS_HPP