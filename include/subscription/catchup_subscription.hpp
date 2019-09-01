#pragma once

#ifndef ES_CATCHUP_SUBSCRIPTION_HPP
#define ES_CATCHUP_SUBSCRIPTION_HPP

#include <deque>

#include "read_all_events.hpp"
#include "read_stream_events.hpp"
#include "subscription_base.hpp"
#include "subscription_settings.hpp"
#include "buffer/buffer_queue.hpp"

namespace es {
namespace subscription {

template <class ConnectionType>
class catchup_subscription
	: public subscription_base<ConnectionType, catchup_subscription<ConnectionType>>
{
public:
	using connection_type = ConnectionType;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;
	using self_type = catchup_subscription<connection_type>;
	using base_type = subscription_base<connection_type, self_type>;

	explicit catchup_subscription(
		std::shared_ptr<connection_type> const& connection,
		op_key_type const& key,
		std::string_view stream,
		std::int64_t from_event_number,
		subscription_settings const& settings
	) : base_type(connection, key, stream),
		current_event_number_(from_event_number),
		settings_(settings),
		event_buffer_()
	{}

	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	void async_start(EventAppearedHandler&& event_appeared, SubscriptionDroppedHandler&& dropped)
	{
		catch_up_events(
			current_event_number_,
			settings_.read_batch_size(),
			[event_appeared = std::forward<EventAppearedHandler>(event_appeared),
			dropped = std::forward<SubscriptionDroppedHandler>(dropped),
			this](std::error_code ec, std::optional<stream_events_slice> result)
		{
			if (!ec)
			{
				auto& value = result.value();

				for (auto& event : value.events())
				{
					event_appeared(event);
					++current_event_number_;
				}

				if (value.is_end_of_stream())
				{
					this->do_async_start(std::move(event_appeared), std::move(dropped));
					return;
				}
				else
				{
					this->async_start(std::move(event_appeared), std::move(dropped));
					return;
				}
			}
			else
			{
				ec = make_error_code(subscription_errors::subscription_request_not_sent);
				this->unsubscribe(ec, std::move(dropped));
				return;
			}
		});
	}

	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	bool on_package_received(
		detail::tcp::tcp_package_view view,
		EventAppearedHandler& event_appeared,
		SubscriptionDroppedHandler& dropped
	)
	{
		if (view.command() == detail::tcp::tcp_command::subscription_confirmation)
		{
			message::SubscriptionConfirmation response;
			response.ParseFromArray(view.data() + view.message_offset(), view.message_size());
			this->set_last_commit_position(response.last_commit_position());
			this->set_is_subscribed(true);
			// for a subscription to a stream (as opposed to the all stream),
			// there should always be an event number
			if (response.has_last_event_number())
			{
				this->set_last_event_number(response.last_event_number());
				if (response.last_event_number() > current_event_number_)
				{
					// catch up all missing events
					std::int64_t count = response.last_event_number() - current_event_number_;
					this->catch_up_missed_events(count, event_appeared, dropped);
				}
			}
			else
			{
				this->unsubscribe(make_error_code(subscription_errors::bad_subscription_confirmation), std::move(dropped));
			}
			
			return true;
		}
		if (view.command() == detail::tcp::tcp_command::stream_event_appeared)
		{
			message::StreamEventAppeared message;
			message.ParseFromArray(view.data() + view.message_offset(), view.message_size());

			event_buffer_.emplace_back(*message.mutable_event());
			//resolved_event resolved{ *message.mutable_event() };
			//std::int64_t event_no = resolved.event().value().event_number();
			std::int64_t event_no = event_buffer_.front().event().value().event_number();

			// this would mean we have missed events
			if (this->last_event_number().value() > current_event_number_) return true;
			
			int i = 0;
			while (!event_buffer_.empty() && i < settings_.read_batch_size())
			{
				event_appeared(event_buffer_.front());
				event_buffer_.pop_front();
				++current_event_number_;
				++i;
			}
		
			return true;
		}

		return false; // will delegate handling to the base class
	}

	void shutdown()
	{
		event_buffer_.clear();
	}

private:
	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	void do_async_start(EventAppearedHandler&& event_appeared, SubscriptionDroppedHandler&& dropped)
	{
		message::SubscribeToStream request;
		request.set_event_stream_id(this->stream());
		request.set_resolve_link_tos(settings_.resolve_link_tos());

		auto serialized = request.SerializeAsString();

		detail::tcp::tcp_package<> package;
		if (this->connection()->settings().default_user_credentials().null())
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::subscribe_to_stream,
				detail::tcp::tcp_flags::none,
				this->correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}
		else
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::subscribe_to_stream,
				detail::tcp::tcp_flags::authenticated,
				this->correlation_id(),
				this->connection()->settings().default_user_credentials().username(),
				this->connection()->settings().default_user_credentials().password(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}

		base_type::async_start(std::forward<EventAppearedHandler>(event_appeared), std::forward<SubscriptionDroppedHandler>(dropped));
		this->connection()->async_send(std::move(package));
	}

	template <class EventSliceReadHandler>
	void catch_up_events(std::int64_t from, std::int64_t count, EventSliceReadHandler&& handler)
	{
		async_read_stream_events(
			this->connection(),
			this->stream(),
			read_direction::forward,
			from,
			(int)count,
			settings_.resolve_link_tos(),
			std::move(handler)
		);
	}

	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	void catch_up_missed_events(std::int64_t count, EventAppearedHandler& event_appeared, SubscriptionDroppedHandler& dropped)
	{
		this->catch_up_events(current_event_number_, count,
			[&event_appeared, &dropped, count = count, this](std::error_code ec, std::optional<stream_events_slice> result)
		{
			if (!ec)
			{
				auto& value = result.value();

				auto mutable_count = count;
				for (auto& event : value.events())
				{
					event_appeared(event);
					++current_event_number_;
					--mutable_count;
				}

				if (mutable_count > 0)
				{
					this->catch_up_missed_events(mutable_count, event_appeared, dropped);
				}

				return;
			}
			else
			{
				this->unsubscribe(ec, std::move(dropped));
				return;
			}
		});
	}
private:
	std::int64_t current_event_number_;
	subscription_settings settings_;
	buffer::buffer_queue<resolved_event, std::deque, 25> event_buffer_;
};

} // subscription

template <class ConnectionType>
auto make_catchup_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	typename ConnectionType::operations_map_type::key_type const& key,
	std::string_view stream,
	std::int64_t from_event_number,
	subscription_settings const& settings) -> std::shared_ptr<subscription::catchup_subscription<ConnectionType>>
{
	return std::make_shared<subscription::catchup_subscription<ConnectionType>>(connection, key, stream, from_event_number, settings);
}

} // es

#endif // ES_CATCHUP_SUBSCRIPTION_HPP