#pragma once

#ifndef ES_CLUSTER_DISCOVERY_SERVICE_HPP
#define ES_CLUSTER_DISCOVERY_SERVICE_HPP

#include <type_traits>
#include <optional>
#include <tuple>
#include <random>

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <nlohmann/json.hpp>

#include "cluster_settings.hpp"
#include "message/cluster_messages.hpp"

namespace es {
namespace tcp {
namespace services {

class cluster_discovery_service
	: public boost::asio::execution_context::service
{
public:
	using endpoint_type = boost::asio::ip::tcp::endpoint;
	using key_type = cluster_discovery_service;
	using node_endpoints_type = std::pair<endpoint_type, endpoint_type>;
	inline static boost::asio::execution_context::id id;

	explicit cluster_discovery_service(
		boost::asio::execution_context& ioc
	) : boost::asio::execution_context::service(ioc),
		settings_(),
		old_gossip_(),
		context_(nullptr)
	{}

	explicit cluster_discovery_service(
		boost::asio::execution_context& ioc,
		cluster_settings const& settings
	)
		: boost::asio::execution_context::service(ioc),
		settings_(settings),
		old_gossip_(),
		context_(nullptr)
	{}

	template <class Connection, class Func>
	bool do_discover_node_endpoints(Connection& connection, Func&& f, int attempt)
	{
		if (attempt <= settings_.max_discover_attempts())
		{
			std::optional<node_endpoints_type> endpoints = discover_endpoints();
			if (endpoints.has_value())
			{
				// for now, we'll just use .first to use the normal tcp endpoint instead of the secure one (ssl not yet used)
				f(boost::system::error_code(), endpoints.value().first);
				return true;
			}
			else
			{
				return false;
			}
		}

		return false;
	}

	template <class Connection, class Func>
	void perform_discovery(Connection& connection, Func&& f, std::shared_ptr<boost::asio::steady_timer> timer, int attempt)
	{
		boost::asio::post(
			connection.get_io_context(),
			[this, timer = timer, &connection, f = std::move(f), attempt = attempt]()
		{
			if (!this->do_discover_node_endpoints(connection, std::move(f), attempt))
			{
				timer->expires_after(std::chrono::milliseconds(500));
				timer->async_wait(
					[this, timer = timer, &connection, f = std::move(f), attempt = attempt + 1 /*here is where we increment the attempt, as in the for loop version of the .net client*/]
				(boost::system::error_code ec)
				{
					if (!ec)
					{
						this->perform_discovery(connection, std::move(f), timer, attempt);
					}
					else
					{
						// do something
					}
				});
			}
		});
	}

	template <class Connection, class Func>
	void async_discover_node_endpoints(Connection& connection, Func&& f)
	{
		static_assert(
			std::is_invocable_v<Func, boost::system::error_code, endpoint_type>,
			"template argument to Func must have signature void(boost::system::error_code, endpoint_type)"
			);

		context_ = std::addressof(connection.get_io_context());
		auto timer = std::make_shared<boost::asio::steady_timer>();
		// start the discovery cycle
		this->perform_discovery(connection, std::forward<Func>(f), timer, 1);
	}

	std::optional<node_endpoints_type> discover_endpoints()
	{
		std::vector<tcp::gossip_seed> candidates = old_gossip_.empty() ? get_dns_candidates() :
			get_candidates(old_gossip_);

		for (int i = 0; i < candidates.size(); ++i)
		{
			std::optional<message::cluster_info> gossip = try_get_gossip_from(candidates[i]);
			if (!gossip.has_value() || gossip.value().members().empty())
				continue;

			std::optional<node_endpoints_type> best_node = try_determine_best_node(gossip.value().members(), settings_.preference());
			if (best_node.has_value())
			{
				old_gossip_ = gossip.value().members();
				return best_node;
			}
		}
	}

	std::vector<tcp::gossip_seed> get_dns_candidates()
	{
		std::vector<tcp::gossip_seed> endpoints;
		if (!settings_.gossip_seeds().empty())
		{
			endpoints = settings_.gossip_seeds();
		}
		else
		{
			// perform dns resolution and create gossip seeds from results
		}

		std::random_device device;
		std::mt19937 gen(device());

		std::shuffle(endpoints.begin(), endpoints.end(), gen);
	}

	std::vector<tcp::gossip_seed> get_candidates(std::vector<message::member_info> const& old_gossip)
	{
		return arrange_gossip_candidates(old_gossip);
	}

	std::vector<tcp::gossip_seed> arrange_gossip_candidates(std::vector<message::member_info> const& members)
	{
		char* buffer = new char[members.size() * sizeof(tcp::gossip_seed)];
		int i = -1;
		int j = members.size();
		for (int k = 0; k < members.size(); ++k)
		{
			auto endpoint = boost::asio::ip::tcp::endpoint(
				boost::asio::ip::make_address_v4(members[k].external_http_ip()),
				members[k].external_http_port()
			);

			if (members[k].state() == message::virtual_node_state::manager)
			{
				new (&buffer[(--j) * sizeof(tcp::gossip_seed)]) tcp::gossip_seed(endpoint);
			}
			else
			{
				new (&buffer[(++i) * sizeof(tcp::gossip_seed)]) tcp::gossip_seed(endpoint);
			}
		}

		tcp::gossip_seed* ptr = reinterpret_cast<tcp::gossip_seed*>(&buffer[0]);
		std::vector<tcp::gossip_seed> result;
		std::copy(ptr, ptr + members.size(), result.begin());

		std::random_device device;
		std::mt19937 gen(device());

		std::shuffle(result.begin(), result.begin() + i, gen);
		std::shuffle(result.begin() + j, result.end(), gen);

		delete[] buffer;

		return result;
	}

	auto try_get_gossip_from(tcp::gossip_seed const& seed) 
		-> std::optional<message::cluster_info>
	{
		std::ostringstream oss{};
		oss << "http://"
			<< seed.endpoint()
			<< "/gossip?format=json";
		std::string url = oss.str();

		namespace beast = boost::beast;
		beast::tcp_stream stream{ *context_ };

		boost::system::error_code ec;
		
		//stream.connect(seed.endpoint());


	}

	auto try_determine_best_node(std::vector<message::member_info> const& members, node_preference preference)
		-> std::optional<node_endpoints_type>
	{
		static std::array<message::virtual_node_state, 3> unallowed_states = 
		{ 
			message::virtual_node_state::manager,
			message::virtual_node_state::shutting_down, 
			message::virtual_node_state::shutdown 
		};

		std::vector<message::member_info> nodes = members;
		// remove if node is not alive or if it is in an unallowed state
		nodes.erase(std::remove(nodes.begin(), nodes.end(), 
			[](message::member_info const& info) 
		{ 
			return !info.is_alive() ||
				std::find_if(unallowed_states.begin(), unallowed_states.end(),
					[&info](auto state) { return info.state() == state; }) != unallowed_states.end();
		}));

		// sort in descending order by node state
		std::sort(nodes.begin(), nodes.end(), 
			[](message::member_info const& info1, message::member_info const& info2)
		{
			return info1.state() > info2.state();
		});

		std::random_device device;
		std::mt19937 gen(device());
		// randomly shuffle nodes
		if (settings_.preference() == node_preference::random)
		{
			std::shuffle(nodes.begin(), nodes.end(), gen);
		}
		if (settings_.preference() == node_preference::slave)
		{
			// put all non-slaves in front
			std::stable_partition(nodes.begin(), nodes.end(), 
				[](message::member_info const& info)
			{
				return info.state() != message::virtual_node_state::slave;
			});

			std::shuffle(
				nodes.begin(),
				nodes.begin() + std::count_if(
					nodes.begin(), nodes.end(), 
					[](message::member_info const& info) { return info.state() == message::virtual_node_state::slave; }
				),
				gen
			);
		}

		// get first node as potential best node
		auto& best_node = *nodes.begin();

		// if node had no info, return nullopt
		if (best_node == message::member_info::null()) // the default member_info
		    return {};

		// else we've found our best node!
		node_endpoints_type endpoints;
		endpoints.first = boost::asio::ip::tcp::endpoint(
			boost::asio::ip::make_address_v4(best_node.external_tcp_ip()), best_node.external_tcp_port()
		);

		endpoints.second = boost::asio::ip::tcp::endpoint(
			boost::asio::ip::make_address_v4(best_node.external_tcp_ip()), best_node.external_secure_tcp_port()
		);

		return std::make_optional(endpoints);
	}

	~cluster_discovery_service()
	{
		shutdown();
	}
private:
	virtual void shutdown() noexcept override
	{}

	cluster_settings settings_;
	std::vector<message::member_info> old_gossip_;
	boost::asio::io_context* context_;
};

} // services
} // tcp
} // es

#endif // ES_CLUSTER_DISCOVERY_SERVICE_HPP