#pragma once

#ifndef ES_CLUSTER_MESSAGES_HPP
#define ES_CLUSTER_MESSAGES_HPP

#include <string>
#include <chrono>

#include <nlohmann/json.hpp>

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

inline const char* to_string(virtual_node_state state)
{
	switch (state)
	{
	case virtual_node_state::initializing:
		return "Initializing";
	case virtual_node_state::unknown:
		return "Unknown";
	case virtual_node_state::pre_replica:
		return "PreReplica";
	case virtual_node_state::catching_up:
		return "CatchingUp";
	case virtual_node_state::clone:
		return "Clone";
	case virtual_node_state::slave:
		return "Slave";
	case virtual_node_state::pre_master:
		return "PreMaster";
	case virtual_node_state::master:
		return "Master";
	case virtual_node_state::manager:
		return "Manager";
	case virtual_node_state::shutting_down:
		return "ShuttingDown";
	case virtual_node_state::shutdown:
		return "Shutdown";
	default:
		return "";
	}
}

inline virtual_node_state from_string(std::string const& str)
{
	if (str == "Initializing")
		return virtual_node_state::initializing;
	else if (str == "Unknown")
		return virtual_node_state::unknown;
	else if (str == "PreReplica")
		return virtual_node_state::pre_replica;
	else if (str == "CatchingUp")
		return virtual_node_state::catching_up;
	else if (str == "Clone")
		return virtual_node_state::clone;
	else if (str == "Slave")
		return virtual_node_state::slave;
	else if (str == "PreMaster")
		return virtual_node_state::pre_master;
	else if (str == "Master")
		return virtual_node_state::master;
	else if (str == "Manager")
		return virtual_node_state::manager;
	else if (str == "ShuttingDown")
		return virtual_node_state::shutting_down;
	else if (str == "Shutdown")
		return virtual_node_state::shutdown;
	else
		// should never happen
		return (virtual_node_state)-1;
}

class member_info
{
public:
	member_info() = default;
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

	/*
	An example response from poking a cluster node's gossip seed
{
	"members": [
		{
			"instanceId": "97a8ae2c-1bd4-4bab-a117-fc2ca134e5b9",
			"timeStamp": "2019-09-05T19:12:37.8141961Z",
			"state": "Slave",
			"isAlive": true,
			"internalTcpIp": "127.0.0.1",
			"internalTcpPort": 5111,
			"internalSecureTcpPort": 0,
			"externalTcpIp": "127.0.0.1",
			"externalTcpPort": 5112,
			"externalSecureTcpPort": 0,
			"internalHttpIp": "127.0.0.1",
			"internalHttpPort": 5113,
			"externalHttpIp": "127.0.0.1",
			"externalHttpPort": 5114,
			"lastCommitPosition": 67611,
			"writerCheckpoint": 85117,
			"chaserCheckpoint": 85117,
			"epochPosition": 1963,
			"epochNumber": 1,
			"epochId": "3e15906c-598b-4146-846e-b8b4bf35e72a",
			"nodePriority": 0
		},
		{
			"instanceId": "9b1350ea-4f62-4008-b2ed-0548712baed2",
			"timeStamp": "2019-09-05T19:12:38.2676819Z",
			"state": "Master",
			"isAlive": true,
			"internalTcpIp": "127.0.0.1",
			"internalTcpPort": 4111,
			"internalSecureTcpPort": 0,
			"externalTcpIp": "127.0.0.1",
			"externalTcpPort": 4112,
			"externalSecureTcpPort": 0,
			"internalHttpIp": "127.0.0.1",
			"internalHttpPort": 4113,
			"externalHttpIp": "127.0.0.1",
			"externalHttpPort": 4114,
			"lastCommitPosition": 85117,
			"writerCheckpoint": 101017,
			"chaserCheckpoint": 101017,
			"epochPosition": 1963,
			"epochNumber": 1,
			"epochId": "3e15906c-598b-4146-846e-b8b4bf35e72a",
			"nodePriority": 0
		},
		{
			"instanceId": "14ae5a92-b51a-430c-b16f-d1597d62db15",
			"timeStamp": "2019-09-05T19:12:38.3656822Z",
			"state": "Slave",
			"isAlive": true,
			"internalTcpIp": "127.0.0.1",
			"internalTcpPort": 3111,
			"internalSecureTcpPort": 0,
			"externalTcpIp": "127.0.0.1",
			"externalTcpPort": 3112,
			"externalSecureTcpPort": 0,
			"internalHttpIp": "127.0.0.1",
			"internalHttpPort": 3113,
			"externalHttpIp": "127.0.0.1",
			"externalHttpPort": 3114,
			"lastCommitPosition": 85117,
			"writerCheckpoint": 101017,
			"chaserCheckpoint": 101017,
			"epochPosition": 1963,
			"epochNumber": 1,
			"epochId": "3e15906c-598b-4146-846e-b8b4bf35e72a",
			"nodePriority": 0
		}
	],
	"serverIp": "127.0.0.1",
	"serverPort": 3113
}
*/
	friend void to_json(nlohmann::json& j, const member_info& m) {
		j = nlohmann::json{ 
			{"instanceId", es::to_string(m.instance_id_) },
		    {"timeStamp", m.date_time_ }, 
		    {"state", to_string(m.state_) },
		    {"isAlive", m.is_alive_},
		    {"internalTcpIp", m.internal_tcp_ip_},
		    {"internalTcpPort", m.internal_tcp_port_},
		    {"internalSecureTcpPort", m.internal_secure_tcp_port_},
		    {"externalTcpIp", m.external_tcp_ip_},
		    {"externalTcpPort", m.external_tcp_port_},
		    {"externalSecureTcpPort", m.external_secure_tcp_port_},
		    {"internalHttpIp", m.internal_http_ip_},
		    {"internalHttpPort", m.internal_http_port_},
		    {"externalHttpIp", m.external_http_ip_},
		    {"externalHttpPort", m.external_http_port_},
		    {"lastCommitPosition", m.last_commit_position_},
		    {"writerCheckpoint", m.writer_checkpoint_},
		    {"chaserCheckpoint", m.chaser_checkpoint_},
		    {"epochPosition", m.epoch_position_},
			{"epochNumber", m.epoch_number_},
			{"epochId", es::to_string(m.epoch_id_) },
			{"nodePriority", m.node_priority_}
		};
	}

	friend void from_json(const nlohmann::json& j, member_info& m) {
		if (j.empty()) return;
		
		auto id = j.at("instanceId").get<std::string>();
		m.instance_id_ = es::guid(std::string_view{ id });

		auto eid = j.at("epochId").get<std::string>();
		m.epoch_id_= es::guid(std::string_view{ eid });
	
		auto state = j.at("state").get<std::string>();
		if ((int)from_string(state) == -1) return;
		m.state_ = from_string(state);
		
		j.at("timeStamp").get_to(m.date_time_);
		j.at("isAlive").get_to(m.is_alive_);
		j.at("internalTcpIp").get_to(m.internal_tcp_ip_);
		j.at("internalTcpPort").get_to(m.internal_tcp_port_);
		j.at("internalSecureTcpPort").get_to(m.internal_secure_tcp_port_);
		j.at("externalTcpIp").get_to(m.external_tcp_ip_);
		j.at("externalTcpPort").get_to(m.external_tcp_port_);
		j.at("externalSecureTcpPort").get_to(m.external_secure_tcp_port_);
		j.at("internalHttpIp").get_to(m.internal_http_ip_);
		j.at("internalHttpPort").get_to(m.internal_http_port_);
		j.at("externalHttpIp").get_to(m.external_http_ip_);
		j.at("externalHttpPort").get_to(m.external_http_port_);
		j.at("lastCommitPosition").get_to(m.last_commit_position_);
		j.at("writerCheckpoint").get_to(m.writer_checkpoint_);
		j.at("chaserCheckpoint").get_to(m.chaser_checkpoint_);
		j.at("epochPosition").get_to(m.epoch_position_);
		j.at("epochNumber").get_to(m.epoch_number_);
		j.at("nodePriority").get_to(m.node_priority_);
	}

private:

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

	friend void to_json(nlohmann::json& j, const cluster_info& c) {
		j = nlohmann::json{ {"members", c.members_} };
	}

	friend void from_json(const nlohmann::json& j, cluster_info& c) {
		j.at("members").get_to(c.members_);
	}
private:
	std::vector<member_info> members_;
};

}
}

#endif // ES_CLUSTER_MESSAGES_HPP