#pragma once

#ifndef ES_VOLATILE_SUBSCRIPTION_HPP
#define ES_VOLATILE_SUBSCRIPTION_HPP

#include "resolved_event.hpp"
#include "subscription_base.hpp"

namespace es {
namespace subscription {

template <class ConnectionType>
class volatile_subscription
	: public subscription_base<ConnectionType, volatile_subscription<ConnectionType>>
{
public:
	using connection_type = ConnectionType;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;
	using self_type = volatile_subscription<connection_type>;
	using base_type = subscription_base<connection_type, self_type>;

	explicit volatile_subscription(
		std::shared_ptr<connection_type> const& connection,
		op_key_type const& key,
		std::string_view stream,
		bool resolve_link_tos = true
	) : base_type(connection, key, stream),
		resolve_link_tos_(resolve_link_tos)
	{}

	template <class EventAppearedHandler, class SubscriptionDroppedHandler>
	void async_start(EventAppearedHandler&& event_appeared, SubscriptionDroppedHandler&& dropped)
	{
		message::SubscribeToStream request;
		request.set_event_stream_id(this->stream());
		request.set_resolve_link_tos(resolve_link_tos_);

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

	template <class EventAppearedHandler>
	bool on_package_received(
		detail::tcp::tcp_package_view view, 
		EventAppearedHandler& event_appeared
	)
	{
		if (view.command() == detail::tcp::tcp_command::subscription_confirmation)
		{
			message::SubscriptionConfirmation response;
			response.ParseFromArray(view.data() + view.message_offset(), view.message_size());
			this->set_last_commit_position(response.last_commit_position());
			if (response.has_last_event_number())
				this->set_last_event_number(response.last_event_number());
			
			return true;
		}
		if (view.command() == detail::tcp::tcp_command::stream_event_appeared)
		{
			message::StreamEventAppeared message;
			message.ParseFromArray(view.data() + view.message_offset(), view.message_size());
			event_appeared(resolved_event(*message.mutable_event()));

			return true;
		}

		return false; // will delegate handling to the base class
	}

private:
	bool resolve_link_tos_;
};

} // subscription

template <class ConnectionType>
auto make_volatile_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	typename ConnectionType::operations_map_type::key_type const& key,
	std::string_view stream,
	bool resolve_link_tos = true) -> std::shared_ptr<subscription::volatile_subscription<ConnectionType>>
{
	return std::make_shared<subscription::volatile_subscription<ConnectionType>>(connection, key, stream, resolve_link_tos);
}

template <class ConnectionType>
auto make_volatile_all_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	typename ConnectionType::operations_map_type::key_type const& key,
	bool resolve_link_tos = true) -> std::shared_ptr<subscription::volatile_subscription<ConnectionType>>
{
	return std::make_shared<subscription::volatile_subscription<ConnectionType>>(connection, key, "", resolve_link_tos);
}

} // es

#endif // ES_VOLATILE_SUBSCRIPTION_HPP