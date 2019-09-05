#pragma once

#ifndef ES_PERSISTENT_SUBSCRIPTION_HPP
#define ES_PERSISTENT_SUBSCRIPTION_HPP

#include "resolved_event.hpp"
#include "subscription_base.hpp"

namespace es {
namespace subscription {

template <class ConnectionType>
class persistent_subscription
	: public subscription_base<ConnectionType, persistent_subscription<ConnectionType>>
{
public:
	// order is important, see messages.pb.h at PersistentSubscriptionNakEvents::NakAction
	enum class nak_event_action
	{
		// client unknown on action, let server decide
		unknown = 0,
		// park message do not resend, put on poison queue
		park = 1,
		// explicitly retry the message
		retry = 2,
		// skip this message do not resend do not put in poison queue
		skip = 3,
		// stop the subscription
		stop = 4
	};

	using connection_type = ConnectionType;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;
	using self_type = persistent_subscription<connection_type>;
	using base_type = subscription_base<connection_type, self_type>;

	explicit persistent_subscription(
		std::shared_ptr<connection_type> const& connection,
		op_key_type const& key,
		std::string_view stream,
		std::string_view group_name,
		unsigned int allowed_in_flight_messages,
		bool auto_ack = true
	) : base_type(connection, key, stream),
		group_name_(group_name),
		subscription_id_(),
		allowed_in_flight_messages_(allowed_in_flight_messages),
		auto_ack_(auto_ack)
	{}

	std::string const& group_name() const { return group_name_; }
	unsigned int allowed_in_flight_messages() const { return allowed_in_flight_messages_; }

	template <class PersistentSubscriptionEventAppearedHandler, class SubscriptionDroppedHandler>
	void async_start(PersistentSubscriptionEventAppearedHandler&& event_appeared, SubscriptionDroppedHandler&& dropped)
	{
		static_assert(
			std::is_invocable_v<PersistentSubscriptionEventAppearedHandler, resolved_event const&, std::int32_t>,
			"PersistentSubscriptionEventAppearedHandler requirements not met, must have signature R(es::resolved_event const&, std::int32_t)"
		);

		message::ConnectToPersistentSubscription request;
		request.set_allowed_in_flight_messages(allowed_in_flight_messages_);
		request.set_event_stream_id(this->stream());
		request.set_subscription_id(group_name_);

		auto serialized = request.SerializeAsString();

		detail::tcp::tcp_package<> package;
		if (this->connection()->settings().default_user_credentials().null())
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::connect_to_persistent_subscription,
				detail::tcp::tcp_flags::none,
				this->correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}
		else
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::connect_to_persistent_subscription,
				detail::tcp::tcp_flags::authenticated,
				this->correlation_id(),
				this->connection()->settings().default_user_credentials().username(),
				this->connection()->settings().default_user_credentials().password(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}

		base_type::async_start(std::forward<PersistentSubscriptionEventAppearedHandler>(event_appeared), std::forward<SubscriptionDroppedHandler>(dropped));
		this->connection()->async_send(std::move(package));
	}

	template <class PersistentSubscriptionEventAppearedHandler, class SubscriptionDroppedHandler>
	bool on_package_received(
		detail::tcp::tcp_package_view view,
		PersistentSubscriptionEventAppearedHandler& event_appeared,
		SubscriptionDroppedHandler& // not used in persistent subscription
	)
	{
		switch (view.command())
		{
		case detail::tcp::tcp_command::persistent_subscription_confirmation:
		{
			message::PersistentSubscriptionConfirmation response;
			response.ParseFromArray(view.data() + view.message_offset(), view.message_size());
			this->set_last_commit_position(response.last_commit_position());
			this->set_is_subscribed(true);
			if (response.has_last_event_number())
				this->set_last_event_number(response.last_event_number());

			// doesn't seem necessary since its value should be the same as group_name_
			subscription_id_ = response.subscription_id();

			return true;
		}
		case detail::tcp::tcp_command::persistent_subscription_stream_event_appeared:
		{
			static message::PersistentSubscriptionStreamEventAppeared message;
			message.ParseFromArray(view.data() + view.message_offset(), view.message_size());
			
			resolved_event resolved{ *message.mutable_event() };
			event_appeared(resolved, (std::int32_t)message.retrycount());
			if (auto_ack_)
			{
				this->acknowledge(resolved);
			}

			return true;
		}
		case detail::tcp::tcp_command::subscription_dropped:
		{
			message::SubscriptionDropped message;
			message.ParseFromArray(view.data() + view.message_offset(), view.message_size());

			boost::system::error_code ec;
			if (message.reason() == message::SubscriptionDropped_SubscriptionDropReason_AccessDenied)
			{
				ec = make_error_code(subscription_errors::access_denied);
			}
			else if (message.reason() == message::SubscriptionDropped_SubscriptionDropReason_NotFound)
			{
				ec = make_error_code(subscription_errors::not_found);
			}
			else if (message.reason() == message::SubscriptionDropped_SubscriptionDropReason_PersistentSubscriptionDeleted)
			{
				ec = make_error_code(subscription_errors::persistent_subscription_deleted);
			}
			else if (message.reason() == message::SubscriptionDropped_SubscriptionDropReason_SubscriberMaxCountReached)
			{
				ec = make_error_code(subscription_errors::subscriber_max_count_reached);
			}
			else
			{
				ec = make_error_code(subscription_errors::unsubscribed);
			}
			
			this->unsubscribe(ec);
			return true;
		}
		default:
			return false;
		}
	}

	template <class GuidOrResolvedEvent>
	void acknowledge(GuidOrResolvedEvent const& correlation_id_or_event)
	{
		static_assert(
			std::is_same_v<GuidOrResolvedEvent, guid_type> || 
			std::is_same_v<GuidOrResolvedEvent, resolved_event>,
			"GuidOrResolvedEvent must be either es::guid_type or es::resolved_event"
		);

		message::PersistentSubscriptionAckEvents ack;
		ack.set_subscription_id(subscription_id_);
		if constexpr (std::is_same_v<GuidOrResolvedEvent, resolved_event>)
		{
			ack.add_processed_event_ids()->assign(
				(char*)correlation_id_or_event.original_event().value().event_id().data,
				correlation_id_or_event.original_event().value().event_id().size()
			);
		}
		else
		{
			ack.add_processed_event_ids()->assign((char*)correlation_id_or_event.data, correlation_id_or_event.size());
		}

		auto serialized = ack.SerializeAsString();

		detail::tcp::tcp_package<> package;

		if (this->connection()->settings().default_user_credentials().null())
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::persistent_subscription_ack_events,
				detail::tcp::tcp_flags::none,
				this->correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}
		else
		{
			package = std::move(detail::tcp::tcp_package<>{
				detail::tcp::tcp_command::persistent_subscription_ack_events,
				detail::tcp::tcp_flags::authenticated,
				this->correlation_id(),
				this->connection()->settings().default_user_credentials().username(),
				this->connection()->settings().default_user_credentials().password(),
				(std::byte*)serialized.data(),
				serialized.size()
				});
		}

		this->connection()->async_send(std::move(package));
	}

	template <class ResolvedEventOrGuidIterator>
	void acknowledge(ResolvedEventOrGuidIterator it, ResolvedEventOrGuidIterator end)
	{
		static_assert(
			(std::is_same_v< std::decay_t<decltype(*it)>, guid_type> ||
				std::is_same_v<std::decay_t<decltype(*it)>, resolved_event>) &&
				(std::is_same_v< std::decay_t<decltype(*end)>, guid_type> ||
					std::is_same_v<std::decay_t<decltype(*end)>, resolved_event>),
			"a dereferenced and const-volatile-reference removed ResolvedEventOrGuidIterator must be of type es::guid_type or es::resolved_event"
		);

		message::PersistentSubscriptionAckEvents ack;
		ack.set_subscription_id(subscription_id_);

		for (; it != end; ++it)
		{
			if constexpr (std::is_same_v<std::decay_t<decltype(*it)>, resolved_event>)
			{
				if constexpr (std::is_same_v<decltype((char*)it->original_event().value().event_id().data), char*>)
				{
					ack.add_processed_event_ids()->assign(
						(char*)it->original_event().value().event_id().data,
						it->original_event().value().event_id().size()
					);
				}
				else
				{
					ack.add_processed_event_ids()->assign(
						(char*)it->original_event().value().event_id().data(),
						it->original_event().value().event_id().size()
					);
				}
			}
			else
			{
				if constexpr (std::is_same_v<decltype((char*)it->data), char*>)
				{
					ack.add_processed_event_ids()->assign((char*)it->data, it->size());
				}
				else
				{
					ack.add_processed_event_ids()->assign((char*)it->data(), it->size());
				}
			}
		}

		auto serialized = ack.SerializeAsString();

		detail::tcp::tcp_package<> package;

		if (this->connection()->settings().default_user_credentials().null())
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::persistent_subscription_ack_events,
				detail::tcp::tcp_flags::none,
				this->correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}
		else
		{
			package = std::move(detail::tcp::tcp_package<>{
				detail::tcp::tcp_command::persistent_subscription_ack_events,
					detail::tcp::tcp_flags::authenticated,
					this->correlation_id(),
					this->connection()->settings().default_user_credentials().username(),
					this->connection()->settings().default_user_credentials().password(),
					(std::byte*)serialized.data(),
					serialized.size()
			});
		}

		this->connection()->async_send(std::move(package));
	}

	// no support for the "reason" argument yet, it is optional in the protobuf contract
	template <class GuidOrResolvedEvent>
	void fail(GuidOrResolvedEvent const& correlation_id_or_event, nak_event_action action)
	{
		static_assert(
			std::is_same_v<GuidOrResolvedEvent, guid_type> || 
			std::is_same_v<GuidOrResolvedEvent, resolved_event>,
			"GuidOrResolvedEvent must be either es::guid_type or es::resolved_event"
		);

		message::PersistentSubscriptionNakEvents nak;
		nak.set_subscription_id(subscription_id_);
		nak.set_action(static_cast<message::PersistentSubscriptionNakEvents::NakAction>(action));
		if constexpr (std::is_same_v<GuidOrResolvedEvent, resolved_event>)
		{
			nak.add_processed_event_ids()->assign(
				(char*)correlation_id_or_event.original_event().value().event_id().data,
				correlation_id_or_event.original_event().value().event_id().size()
			);
		}
		else
		{
			nak.add_processed_event_ids()->assign((char*)correlation_id_or_event.data, correlation_id_or_event.size());
		}

		auto serialized = nak.SerializeAsString();

		detail::tcp::tcp_package<> package;

		if (this->connection()->settings().default_user_credentials().null())
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::persistent_subscription_nak_events,
				detail::tcp::tcp_flags::none,
				this->correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}
		else
		{
			package = std::move(detail::tcp::tcp_package<>{
				detail::tcp::tcp_command::persistent_subscription_nak_events,
					detail::tcp::tcp_flags::authenticated,
					this->correlation_id(),
					this->connection()->settings().default_user_credentials().username(),
					this->connection()->settings().default_user_credentials().password(),
					(std::byte*)serialized.data(),
					serialized.size()
			});
		}

		this->connection()->async_send(std::move(package));
	}

	template <class ResolvedEventOrGuidIterator>
	void fail(ResolvedEventOrGuidIterator it, ResolvedEventOrGuidIterator end, nak_event_action action)
	{
		static_assert(
			(std::is_same_v< std::decay_t<decltype(*it)>, guid_type> ||
				std::is_same_v<std::decay_t<decltype(*it)>, resolved_event>) &&
				(std::is_same_v< std::decay_t<decltype(*end)>, guid_type> ||
					std::is_same_v<std::decay_t<decltype(*end)>, resolved_event>),
			"a dereferenced and const-volatile-reference removed ResolvedEventOrGuidIterator must be of type es::guid_type or es::resolved_event"
		);

		message::PersistentSubscriptionNakEvents nak;
		nak.set_subscription_id(subscription_id_);
		nak.set_action(static_cast<message::PersistentSubscriptionNakEvents::NakAction>(action));
		
		for (; it != end; ++it)
		{
			// if the iterator's value_type is a resolved_event
			if constexpr (std::is_same_v<std::decay_t<decltype(*it)>, resolved_event>)
			{
				if constexpr (std::is_same_v<decltype((char*)it->original_event().value().event_id().data), char*>)
				{
					nak.add_processed_event_ids()->assign(
						(char*)it->original_event().value().event_id().data,
						it->original_event().value().event_id().size()
					);
				}
				else
				{
					nak.add_processed_event_ids()->assign(
						(char*)it->original_event().value().event_id().data(),
						it->original_event().value().event_id().size()
					);
				}
			}
			// iterator's value_type should be es::guid_type
			else
			{
				if constexpr (std::is_same_v<decltype((char*)it->data), char*>)
				{
					nak.add_processed_event_ids()->assign((char*)it->data, it->size());
				}
				else
				{
					nak.add_processed_event_ids()->assign((char*)it->data(), it->size());
				}
			}
		}

		auto serialized = nak.SerializeAsString();

		detail::tcp::tcp_package<> package;

		if (this->connection()->settings().default_user_credentials().null())
		{
			package = std::move(detail::tcp::tcp_package<>(
				detail::tcp::tcp_command::persistent_subscription_nak_events,
				detail::tcp::tcp_flags::none,
				this->correlation_id(),
				(std::byte*)serialized.data(),
				serialized.size()
				));
		}
		else
		{
			package = std::move(detail::tcp::tcp_package<>{
				detail::tcp::tcp_command::persistent_subscription_nak_events,
					detail::tcp::tcp_flags::authenticated,
					this->correlation_id(),
					this->connection()->settings().default_user_credentials().username(),
					this->connection()->settings().default_user_credentials().password(),
					(std::byte*)serialized.data(),
					serialized.size()
			});
		}

		this->connection()->async_send(std::move(package));
	}

private:
	// why do we have both group name and subscription id ? aren't they the same?
	std::string group_name_;
	std::string subscription_id_;
	unsigned int allowed_in_flight_messages_;
	bool auto_ack_;
};

} // subscription

template <class ConnectionType>
auto make_persistent_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	typename ConnectionType::operations_map_type::key_type const& key,
	std::string_view stream,
	std::string_view group_name,
	unsigned int allowed_in_flight_messages,
	bool auto_ack = true
) -> std::shared_ptr<subscription::persistent_subscription<ConnectionType>>
{
	return std::make_shared<subscription::persistent_subscription<ConnectionType>>(
		connection,
		key,
		stream,
		group_name,
		allowed_in_flight_messages,
		auto_ack
	);
}

} // es

#endif // ES_PERSISTENT_SUBSCRIPTION_HPP