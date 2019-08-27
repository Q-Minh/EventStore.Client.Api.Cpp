#include <string>
#include <mutex>
#include <condition_variable>

#include <asio/io_context.hpp>
#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/address.hpp>
#include <asio/buffer.hpp>

#include "logger.hpp"
#include "connection_settings.hpp"
#include "event_data.hpp"
#include "append_to_stream.hpp"
#include "read_stream_event.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "error/error.hpp"
#include "tcp/discovery_service.hpp"
#include "user/user_credentials.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 6)
	{
		ES_ERROR("expected 5 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> [trace | debug | info | warn | error | critical | off]");
		return 0;
	}

	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::string_view lvl = argv[5];
	ES_DEFAULT_LOG_LEVEL(lvl);

	try
	{
		asio::io_context ioc;

		asio::ip::tcp::endpoint endpoint;
		endpoint.address(asio::ip::make_address_v4(ep));
		endpoint.port(port);
		
		es::user::user_credentials credentials(username, password);

		auto connection_settings =
			es::connection_settings_builder()
			.with_default_user_credentials(credentials)
			.build();

		using discovery_service_type = es::tcp::services::discovery_service;
		auto& discovery_service = asio::make_service<discovery_service_type>(ioc, endpoint, asio::ip::tcp::endpoint(), false);
		
		using connection_type = 
			es::connection::basic_tcp_connection<
				asio::steady_timer, 
				discovery_service_type, 
				es::operation<>
			>;

		std::vector<std::uint8_t> buffer_storage;

		std::shared_ptr<connection_type> tcp_connection = 
			std::make_shared<connection_type>(
				ioc, 
				connection_settings, 
				asio::dynamic_buffer(buffer_storage)
			);

		bool is_connected;
		bool notification;
		std::mutex mutex;
		std::condition_variable cv;

		tcp_connection->async_connect(
			[tcp_connection=tcp_connection, &is_connected, &notification, &mutex, &cv](asio::error_code ec, es::detail::tcp::tcp_package_view view)
		{
			std::lock_guard<std::mutex> lock(mutex);
			notification = true;

			if (!ec || ec == es::connection_errors::authentication_failed)
			{
				// on success, command should be client identified
				if (view.command() != es::detail::tcp::tcp_command::client_identified) return;

				ES_INFO("client has been identified, {}", ec.message());
				is_connected = true;
			}
			else
			{
				// es connection failed
				ES_ERROR("client has failed to identify with server, {}", ec.message());
			}

			cv.notify_one();
		}
		);

		// run io_context in a single thread, it is safe to use io_context concurrently
		std::thread message_pump{ [&ioc]() { ioc.run(); } };

		// wait for server connection response
		{
			std::unique_lock<std::mutex> lock(mutex);
			cv.wait(lock, [&notification] { return notification; });
		}

		if (!is_connected)
		{
			return 0;
		}

		std::vector<es::event_data> events;
		auto guid = es::guid();
		es::event_data event{ guid, "Test.Type", true, "{ \"test\": \"data\"}", "test metadata" };
		events.push_back(event);
		std::string stream_name = "test-stream";

		notification = false;

		es::async_append_stream(
			*tcp_connection,
			stream_name,
			std::move(events),
			[tcp_connection = tcp_connection, guid, stream_name, &notification, &mutex, &cv](std::error_code ec, std::optional<es::write_result> result)
		{
			if (!ec)
			{
				ES_INFO("successfully appended to stream {}, next expected version={}", stream_name, result.value().next_expected_version());
			}
			else
			{
				ES_ERROR("error appending to stream : {}", ec.message());
				return;
			}
		}
		);

		std::int64_t event_number = 0;
		bool resolve_links_tos = true;
		es::async_read_stream_event(
			*tcp_connection,
			stream_name,
			event_number,
			resolve_links_tos,
			[tcp_connection = tcp_connection, stream = stream_name, event_no = event_number](std::error_code ec, std::optional<es::event_read_result> result)
		{
			if (!ec)
			{
				auto read_result = result.value();
				ES_INFO("read event {} from stream {}", read_result.event_number(), read_result.stream());

				if (!read_result.event().has_value()) return;
				auto event = read_result.event().value();

				ES_INFO("stream-id={}\n\tresolved={}\n\tevent-number={}\n\tevent-id={}\n\tis-json={}\n\tevent-type={}\n\tcreated={}\n\tcreated_epoch={}\n\tmetadata={}\n\tdata={}", 
					event.event().stream_id(),
					event.is_resolved(),
					event.event().event_number(),
					es::to_string(event.event().event_id()),
					event.event().is_json(),
					event.event().event_type(),
					event.event().created(),
					event.event().created_epoch(),
					event.event().metadata(),
					event.event().content()
				);
			}
			else
			{
				ES_ERROR("error trying to read event {} from stream {}, {}", event_no, stream, ec.message());
				return;
			}
		}
		);

		message_pump.join();
	}
	catch (std::exception const& e)
	{
		ES_ERROR(e.what());
	}

    return 0;
}