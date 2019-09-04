#pragma once

#ifndef ES_CLUSTER_MESSAGES_HPP
#define ES_CLUSTER_MESSAGES_HPP

#include <string>
#include <chrono>

#include "guid.hpp"

namespace es {
namespace message {

enum class virtual_node_state
{
	initializing,
	unknown,
	pre_replica,
	catching_up,
	clone,
	slave,
	pre_master,
	master,
	manager,
	shutting_down,
	shutdown
};

class member_info
{
public:
	static member_info null() { return member_info(); }
	guid_type const& instance_id() const { return instance_id_; }
	std::string const& date_time() const { return date_time_; }
	virtual_node_state state() const { return state_; }
	bool is_alive() const { return is_alive_; }
	std::string const& internal_tcp_ip() const { return internal_tcp_ip_; }
	int internal_tcp_port() const { return internal_tcp_port_; }
	int internal_secure_tcp_port() const { return internal_secure_tcp_port_; }
	std::string const& external_tcp_ip() const { return external_tcp_ip_; }
	int external_tcp_port() const { return external_tcp_port_; }
	int external_secure_tcp_port() const { return external_secure_tcp_port_; }
	std::string const& internal_http_ip() const { return internal_http_ip_; }
	int internal_http_port() const { return internal_http_port_; }
	std::string const& external_http_ip() const { return external_http_ip_; }
	int external_http_port() const { return external_http_port_; }
	std::int64_t last_commit_position() const { return last_commit_position_; }
	std::int64_t writer_checkpoint() const { return writer_checkpoint_; }
	std::int64_t chaser_checkpoint() const { return chaser_checkpoint_; }
	std::int64_t epoch_position() const { return epoch_position_; }
	int epoch_number() const { return epoch_number_; }
	guid_type const& epoch_id() const { return epoch_id_; }
	int node_priority() const { return node_priority_; }

	bool operator==(member_info const& other)
	{
		return instance_id_ == other.instance_id_ &&
			date_time_ == other.date_time_ &&
			state_ == other.state_ &&
			is_alive_ == other.is_alive_ &&
			internal_tcp_ip_ == other.internal_tcp_ip_ &&
			internal_tcp_port_ == other.internal_tcp_port_ &&
			internal_secure_tcp_port_ == other.internal_secure_tcp_port_ &&
			external_tcp_ip_ == other.external_tcp_ip_ &&
			external_tcp_port_ == other.external_tcp_port_ &&
			external_secure_tcp_port_ == other.external_secure_tcp_port_ &&
			internal_http_ip_ == other.internal_http_ip_ &&
			internal_http_port_ == other.internal_http_port_ &&
			external_http_ip_ == other.external_http_ip_ &&
			external_http_port_ == other.external_http_port_ &&
			last_commit_position_ == other.last_commit_position_ &&
			writer_checkpoint_ == other.writer_checkpoint_ &&
			chaser_checkpoint_ == other.chaser_checkpoint_ &&
			epoch_position_ == other.epoch_position_ &&
			epoch_number_ == other.epoch_number_ &&
			epoch_id_ == other.epoch_id_ &&
			node_priority_ == other.node_priority_;
	}

private:
	member_info() = default;

	guid_type instance_id_;
	std::string date_time_; // datetime is iso 8601 format, e.g. "2012-03-21T05:40Z", do we need to parse?
	virtual_node_state state_;
	bool is_alive_;
	std::string internal_tcp_ip_;
	int internal_tcp_port_;
	int internal_secure_tcp_port_;
	std::string external_tcp_ip_;
	int external_tcp_port_;
	int external_secure_tcp_port_;
	std::string internal_http_ip_;
	int internal_http_port_;
	std::string external_http_ip_;
	int external_http_port_;
	std::int64_t last_commit_position_;
	std::int64_t writer_checkpoint_;
	std::int64_t chaser_checkpoint_;
	std::int64_t epoch_position_;
	int epoch_number_;
	guid_type epoch_id_;
	int node_priority_;
};

class cluster_info
{
public:
	std::vector<member_info> const& members() const { return members_; }

private:
	std::vector<member_info> members_;
};

}
}

#endif // ES_CLUSTER_MESSAGES_HPP