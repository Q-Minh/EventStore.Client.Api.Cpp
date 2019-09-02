#pragma once

#ifndef ES_SUBSCRIPTION_BASE_HPP
#define ES_SUBSCRIPTION_BASE_HPP

#include <memory>
#include <optional>

#include <asio/post.hpp>

#include "message/messages.pb.h"

#include "logger.hpp"
#include "guid.hpp"
#include "subscription_settings.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {
namespace subscription {

template <class Subscription, typename = void>
struct has_shutdown : std::false_type {};

template <class Subscription>
struct has_shutdown<Subscription, 
	std::void_t<decltype(std::declval<Subscription>().shutdown())>> 
	: std::true_type {};

template <class ConnectionType, class Derived>
class subscription_base
{
public:
	using connection_type = ConnectionType;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;
	using child_type = Derived;

	explicit subscription_base(
		std::shared_ptr<connection_type> const& connection,
		op_key_type const& key,
		std::string_view stream
	) : connection_(connection), 
		key_(key), 
		stream_(stream), 
		last_event_number_(), 
		last_commit_position_(),
		subscribed_(false),
		handle_guard_(false)
	{}

	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	void async_start(EventAppearedHandler&& event_appeared, SubscriptionDroppedHandler&& dropped)
	{
		this->unlock_handle_guard();

		connection_->subscriptions_map_.register_op(
			key_,
			[event_appeared = std::forward<EventAppearedHandler>(event_appeared), 
			dropped = std::forward<SubscriptionDroppedHandler>(dropped),
			key=key_, this]
		(std::error_code ec, detail::tcp::tcp_package_view view)
		{
			if (handle_guard_) return;

			if (ec)
			{
				this->unsubscribe(ec, std::move(dropped));
				return;
			}
			// let derived class handler package first, and if he can't handle it, 
			// it will return false to delegate it to the base (this class)
			if (static_cast<child_type*>(this)->on_package_received(view, event_appeared, dropped)) return;

			switch (view.command())
			{
			// handle a subscription drop !
			case detail::tcp::tcp_command::subscription_dropped:
			{
				message::SubscriptionDropped response;
				response.ParseFromArray(view.data() + view.message_offset(), view.message_size());
				switch (response.reason())
				{
				case message::SubscriptionDropped_SubscriptionDropReason_Unsubscribed:
					ec = make_error_code(subscription_errors::unsubscribed);
					break;;
				case message::SubscriptionDropped_SubscriptionDropReason_AccessDenied:
					ec = make_error_code(subscription_errors::access_denied);
					break;
				case message::SubscriptionDropped_SubscriptionDropReason_NotFound:
					ec = make_error_code(subscription_errors::not_found);
					break;
				default:
					ec = make_error_code(subscription_errors::unknown);
					return;
				}

				unsubscribe(ec, std::move(dropped));
			}
				return;
			// handle if our subscription request was refused for not authenticated
			case detail::tcp::tcp_command::not_authenticated:
			{
				ec = make_error_code(subscription_errors::not_authenticated);
				unsubscribe(ec, std::move(dropped));
			}
				return;
			// handle if our subscription request was refused for sending a bad request
			case detail::tcp::tcp_command::bad_request:
			{
				ec = make_error_code(communication_errors::bad_request);
				unsubscribe(ec, std::move(dropped));
			}
				return;
			// handle if our subscription request was refused because server couldn't deal with it
			case detail::tcp::tcp_command::not_handled:
			{
				if (subscribed_)
				{
					// not handled received while we were already subscribed
					ec = make_error_code(subscription_errors::unknown);
					unsubscribe(ec, std::move(dropped));
					return;
				}

				message::NotHandled response;
				response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

				switch (response.reason())
				{
				case message::NotHandled_NotHandledReason_NotReady:
					// retry
					break;
				case message::NotHandled_NotHandledReason_TooBusy:
					// retry
					break;
				case message::NotHandled_NotHandledReason_NotMaster:
					// reconnect using master info
					// message::NotHandled::MasterInfo info;
					// info.ParseFromString(response.additional_info());
					// info.external_http_address();
					// info.external_http_port();
					// info.external_secure_tcp_address();
					// info.external_secure_tcp_port();
					// info.external_tcp_address();
					// info.external_tcp_port();
					break;
				default:
					// retry
					break;
				}

				ec = make_error_code(connection_errors::max_operation_retries);
				unsubscribe(ec, std::move(dropped));
				return;
			}
				return;
			default:
				ec = make_error_code(communication_errors::server_error);
				unsubscribe(ec, std::move(dropped));
				return;
			}
		}
		);
	}

	void unsubscribe()
	{
		std::error_code ec = make_error_code(subscription_errors::unsubscribed);
		connection_->subscriptions_map_[key_](ec, {});
	}

	std::string const& stream() const { return stream_; }
	std::optional<std::int64_t> const& last_event_number() const { return last_event_number_; }
	std::optional<std::int64_t> const& last_commit_position() const { return last_commit_position_; }
	bool is_subscribed() const { return subscribed_; }
	std::shared_ptr<connection_type> const& connection() const { return connection_; }

protected:
	// derived classes will deal with these setters

	template <class SubscriptionDroppedHandler>
	void unsubscribe(std::error_code ec, SubscriptionDroppedHandler&& dropped)
	{
		this->set_is_subscribed(false);
		this->lock_handle_guard();
		// uncomment following block to verify if derived class' shutdown() method
		// is actually called
		/*static_assert(
			has_shutdown<Derived>::value,
			"subscriptions derived from subscription base must have a shutdown() method"
		);*/
		if constexpr (has_shutdown<Derived>::value)
		{
			static_cast<Derived*>(this)->shutdown();
		}

		asio::post(
			connection_->get_io_context(),
			[dropped = std::move(dropped),
			ec = ec, key = key_, this]()
		{
			connection_->subscriptions_map_.erase(key); // remove subscription from map
			dropped(ec, *static_cast<child_type*>(this)); // notify server error
		}
		);
	}

	void set_last_event_number(std::int64_t number) { last_event_number_ = number; }
	void set_last_commit_position(std::int64_t position) { last_commit_position_ = position; }
	void set_is_subscribed(bool subscribed) { subscribed_ = subscribed; }

	op_key_type const& correlation_id() const { return key_; }
	void lock_handle_guard() { handle_guard_ = true; }
	void unlock_handle_guard() { handle_guard_ = false; }

private:
	std::shared_ptr<connection_type> connection_; // make this shared or weak ??
	op_key_type key_;
	std::string stream_;
	std::optional<std::int64_t> last_event_number_;
	std::optional<std::int64_t> last_commit_position_;
	bool subscribed_;
	bool handle_guard_;
};

} // subscription
} // es

#endif // ES_SUBSCRIPTION_BASE_HPP