#pragma once

/*
Add user credentials parameter to have authentication per request
*/

#ifndef ES_DELETE_PERSISTENT_SUBSCRIPTION_HPP
#define ES_DELETE_PERSISTENT_SUBSCRIPTION_HPP

#include <string>
#include <optional>

#include "message/messages.pb.h"

#include "guid.hpp"
#include "error/error.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

template <class ConnectionType, class DeletePersistentSubscriptionResultHandler>
void async_delete_persistent_subscription(
	std::shared_ptr<ConnectionType> const& connection,
	std::string_view stream,
	std::string_view group,
	DeletePersistentSubscriptionResultHandler&& handler
)
{
	static_assert(
		std::is_invocable_v<DeletePersistentSubscriptionResultHandler, boost::system::error_code, std::string>,
		"DeletePersistentSubscriptionResultHandler requirements not met, must have signature R(boost::system::error_code, std::string)"
		);

	message::DeletePersistentSubscription request;
	request.set_event_stream_id(std::string(stream));
	request.set_subscription_group_name(std::string(group));
	
	auto serialized = request.SerializeAsString();
	auto corr_id = es::guid();
	detail::tcp::tcp_package<> package;

	if (connection->settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::delete_persistent_subscription,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::delete_persistent_subscription,
			detail::tcp::tcp_flags::authenticated,
			corr_id,
			connection->settings().default_user_credentials().username(),
			connection->settings().default_user_credentials().password(),
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}

	connection->async_send(
		std::move(package),
		[handler = std::move(handler)](boost::system::error_code ec, detail::tcp::tcp_package_view view)
	{
		if (!ec && view.command() != detail::tcp::tcp_command::delete_persistent_subscription_completed)
		{
			ec = make_error_code(communication_errors::unexpected_response);
		}

		// if there was an error, report immediately
		if (ec)
		{
			handler(ec, {});
			return;
		}

		message::DeletePersistentSubscriptionCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());
		
		switch (response.result())
		{
		case message::DeletePersistentSubscriptionCompleted_DeletePersistentSubscriptionResult_Success:
			break;
		case message::DeletePersistentSubscriptionCompleted_DeletePersistentSubscriptionResult_Fail:
			ec = make_error_code(subscription_errors::persistent_subscription_fail);
			break;
		case message::DeletePersistentSubscriptionCompleted_DeletePersistentSubscriptionResult_AccessDenied:
			ec = make_error_code(subscription_errors::access_denied);
			break;
		case message::DeletePersistentSubscriptionCompleted_DeletePersistentSubscriptionResult_DoesNotExist:
			ec = make_error_code(subscription_errors::persistent_subscription_does_not_exist);
			break;
		default:
			ec = make_error_code(communication_errors::unexpected_response);
			break;
		}

		handler(ec, response.has_reason() ? response.reason() : "");
		return;
	}
	);
}

} // es

#endif // ES_DELETE_PERSISTENT_SUBSCRIPTION_HPP