#pragma once

/*
Add user credentials parameter to have authentication per request
*/

#ifndef ES_APPEND_TO_STREAM_HPP
#define ES_APPEND_TO_STREAM_HPP

#include <string>
#include <optional>

#include "message/messages.pb.h"

#include "expected_version.hpp"
#include "logger.hpp"
#include "guid.hpp"
#include "write_result.hpp"
#include "error/error.hpp"
#include "tcp/tcp_commands.hpp"
#include "tcp/tcp_flags.hpp"
#include "tcp/tcp_package.hpp"

namespace es {

template <class ConnectionType, class NewEventSequence, class WriteResultHandler>
void async_append_to_stream(
	ConnectionType& connection,
	std::string const& stream,
	NewEventSequence&& events,
	WriteResultHandler&& handler,
	long expected_version = (long)expected_version::any
)
{
	static_assert(
		std::is_invocable_v<WriteResultHandler, std::error_code, std::optional<write_result>>,
		"WriteResultHandler requirements not met, must have signature R(std::error_code, std::optional<es::write_result>)"
	);

	message::WriteEvents message;
	message.set_event_stream_id(stream);
	message.set_expected_version(expected_version);
	message.set_require_master(connection.settings().require_master());
	
	auto it = std::begin(events);
	auto end = std::end(events);

	for (; it != end; ++it)
	{
		auto* new_event = message.add_events();
		if constexpr (std::is_same_v<es::guid_type, std::decay_t<decltype(it->event_id())>>)
		{
			new_event->set_event_id(it->event_id().data, it->event_id().size());
		}
		else
		{
			new_event->set_event_id(std::move(it->event_id()));
		}

		// we just try to move to prevent copying
		new_event->set_event_type(std::move(it->type()));
		new_event->set_data_content_type(it->is_json() ? 1 : 0);
		new_event->set_metadata_content_type(0); // see .NET client api
		new_event->set_data(std::move(it->content()));
		new_event->set_metadata(std::move(it->metadata()));
	}

	auto serialized = message.SerializeAsString();

	auto corr_id = es::guid();

	detail::tcp::tcp_package<> package;

	if (connection.settings().default_user_credentials().null())
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::write_events,
			detail::tcp::tcp_flags::none,
			corr_id,
			(std::byte*)serialized.data(),
			serialized.size()
			));
	}
	else
	{
		package = std::move(detail::tcp::tcp_package<>(
			detail::tcp::tcp_command::write_events,
			detail::tcp::tcp_flags::authenticated,
			corr_id,
			connection.settings().default_user_credentials().username(),
			connection.settings().default_user_credentials().password(),
			(std::byte*)serialized.data(),
			serialized.size()
		));
	}

	connection.async_send(
		std::move(package),
		[handler = std::move(handler)](std::error_code ec, detail::tcp::tcp_package_view view)
	{
		// if there was an error, report immediately
		if (ec)
		{
			handler(ec, {});
			return;
		}

		message::WriteEventsCompleted response;
		response.ParseFromArray(view.data() + view.message_offset(), view.message_size());

		switch (response.result())
		{
		case message::OperationResult::Success:
			break;
		case message::OperationResult::PrepareTimeout:
			// retry
			return;
		case message::OperationResult::ForwardTimeout:
			// retry
			return;
		case message::OperationResult::CommitTimeout:
			// retry
			return;
		case message::OperationResult::WrongExpectedVersion:
			ec = make_error_code(stream_errors::wrong_expected_version);
			break;
		case message::OperationResult::StreamDeleted:
			ec = make_error_code(stream_errors::stream_deleted);
			break;
		case message::OperationResult::InvalidTransaction:
			ec = make_error_code(stream_errors::invalid_transaction);
			break;
		case message::OperationResult::AccessDenied:
			ec = make_error_code(stream_errors::access_denied);
			break;
		default:
			ec = make_error_code(communication_errors::unexpected_response);
			break;
		}

		if (!ec)
		{
			std::int64_t prepare_position = -1;
			std::int64_t commit_position = -1;
			if (response.has_prepare_position()) prepare_position = response.prepare_position();
			if (response.has_commit_position()) commit_position = response.commit_position();

			auto result = std::make_optional(write_result{
					response.last_event_number(), 
					// prepare position is put as argument to commit position, and vice versa, 
					// is this expected behavior (see .NET client api)?
					position{ prepare_position, commit_position }
				});

			handler(ec, result);
			return;
		}
		else
		{
			// on error, write result is garbage
			handler(ec, {});
			return;
		}
	}
	);
}

}

#endif // ES_APPEND_TO_STREAM_HPP