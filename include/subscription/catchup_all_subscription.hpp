#pragma once

#ifndef ES_CATCHUP_SUBSCRIPTION_HPP
#define ES_CATCHUP_SUBSCRIPTION_HPP

#include <deque>

#include "position.hpp"
#include "read_all_events.hpp"
#include "read_stream_events.hpp"
#include "subscription_base.hpp"
#include "subscription_settings.hpp"
#include "buffer/buffer_queue.hpp"

namespace es {
namespace subscription {

template <class ConnectionType>
class catchup_all_subscription
	: public subscription_base<ConnectionType, catchup_all_subscription<ConnectionType>>
{
public:
	using connection_type = ConnectionType;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;
	using self_type = catchup_all_subscription<connection_type>;
	using base_type = subscription_base<connection_type, self_type>;

	explicit catchup_all_subscription(
		std::shared_ptr<connection_type> const& connection,
		op_key_type const& key,
		position from_position,
		subscription_settings const& settings
	) : base_type(connection, key, ""),
		current_position_(from_position),
		settings_(settings),
		event_buffer_()
	{}

	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	void async_start(EventAppearedHandler&& event_appeared, SubscriptionDroppedHandler&& dropped)
	{
		catch_up_events(
			current_position_,
			settings_.read_batch_size(),
			[event_appeared = std::forward<EventAppearedHandler>(event_appeared),
			dropped = std::forward<SubscriptionDroppedHandler>(dropped),
			this](std::error_code ec, std::optional<all_events_slice> result)
		{
			if (!ec)
			{
				auto& value = result.value();

				for (auto& event : value.events())
				{
					event_appeared(event);
				}
				
				if (value.is_end_of_stream())
				{
					this->do_async_start(std::move(event_appeared), std::move(dropped));
					return;
				}
				else
				{
					current_position_ = value.next_position();
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
			if (response.has_last_event_number())
				this->set_last_event_number(response.last_event_number());

			position_to_catch_up_to_ = position{ response.last_commit_position(), response.last_commit_position() };
			if (position_to_catch_up_to_ > current_position_)
			{
				this->catch_up_missed_events(settings_.read_batch_size(), event_appeared, dropped);
			}

			return true;
		}
		if (view.command() == detail::tcp::tcp_command::stream_event_appeared)
		{
			message::StreamEventAppeared message;
			message.ParseFromArray(view.data() + view.message_offset(), view.message_size());

			//resolved_event resolved{ *message.mutable_event() };
			//position event_pos = resolved.original_position();
			event_buffer_.emplace_back(*message.mutable_event());
			
			// we are currently catching up
			if (position_to_catch_up_to_ > current_position_) return true;

			if (event_buffer_.size() == 1)
			{
				read_subscription_events(event_appeared);
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

	template <class AllEventSliceReadHandler>
	void catch_up_events(position const& from, std::int64_t count, AllEventSliceReadHandler&& handler)
	{
		async_read_all_events(
			this->connection(),
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
		this->catch_up_events(current_position_, count,
			[&event_appeared, &dropped, count = count, this](std::error_code ec, std::optional<all_events_slice> result)
		{
			if (!ec)
			{
				auto& value = result.value();

				bool continue_next_batch = true;
				for (auto& event : value.events())
				{
					current_position_ = event.original_position().value();

					// if we've read up to the first event received from the subscription, 
					// stop catching up
					if (current_position_ == position_to_catch_up_to_)
					{
						continue_next_batch = false;
						break;
					}

					event_appeared(event);
				}

				if (continue_next_batch)
				{
					this->catch_up_missed_events(count, event_appeared, dropped);
				}
				else
				{
					this->read_subscription_events(event_appeared);
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

	template <class EventAppearedHandler>
	void read_subscription_events(EventAppearedHandler& event_appeared)
	{
		int i = 0;
		while (!event_buffer_.empty() && i < settings_.max_live_queue_size())
		{
			current_position_ = event_buffer_.front().original_position().value();
			event_appeared(event_buffer_.front());
			event_buffer_.pop_front();
			++i;
		}

		if (!event_buffer_.empty())
		{
			asio::post(
				this->connection()->get_io_context(),
				[this, &event_appeared]()
			{
				read_subscription_events(event_appeared);
			});
		}
	}
private:
	position current_position_;
	subscription_settings settings_;
	buffer::buffer_queue<resolved_event, std::deque, 25> event_buffer_;
	position position_to_catch_up_to_;
};

} // subscription

template <class ConnectionType>
auto make_catchup_all_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	typename ConnectionType::operations_map_type::key_type const& key,
	position from_position,
	subscription_settings const& settings) -> std::shared_ptr<subscription::catchup_all_subscription<ConnectionType>>
{
	return std::make_shared<subscription::catchup_all_subscription<ConnectionType>>(connection, key, from_position, settings);
}

} // es

#endif // ES_CATCHUP_SUBSCRIPTION_HPP