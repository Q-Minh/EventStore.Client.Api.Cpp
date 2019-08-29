#include <mutex>
#include <condition_variable>

#include <asio/ip/basic_resolver.hpp>
#include <asio/ip/address.hpp>

#include "read_stream_event.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 8)
	{
		ES_ERROR("expected 7 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <stream-name> <event-number> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./read-stream-event 127.0.0.1 1113 admin changeit test-stream 0 info");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::string stream = argv[5];
	std::int64_t event_no = std::stoll(argv[6]);
	std::string_view lvl = argv[7];
	ES_DEFAULT_LOG_LEVEL(lvl);

	asio::io_context ioc;

	asio::ip::tcp::endpoint endpoint;
	endpoint.address(asio::ip::make_address_v4(ep));
	endpoint.port(port);

	// all operation will be authenticated by default
	es::user::user_credentials credentials(username, password);

	auto connection_settings =
		es::connection_settings_builder()
		.with_default_user_credentials(credentials)
		.build();

	// register the discovery service to enable the es tcp connection to discover endpoints to connect to
	// for now, the discovery service doesn't do anything, since we haven't implemented cluster node discovery
	using discovery_service_type = es::tcp::services::discovery_service;
	auto& discovery_service = asio::make_service<discovery_service_type>(ioc, endpoint, asio::ip::tcp::endpoint(), false);

	// parameterize our tcp connection with steady timer, the basic discovery service and our type-erased operation
	using connection_type =
		es::connection::basic_tcp_connection<
		asio::steady_timer,
		discovery_service_type,
		es::operation<>
		>;

	// hold storage for the tcp connection to read into
	// this is not necessary, we only need to do this because asio's dynamic buffer
	// does not own the storage, but any type satisfying DynamicBuffer requirements
	// will do
	std::vector<std::uint8_t> buffer_storage;

	std::shared_ptr<connection_type> tcp_connection =
		std::make_shared<connection_type>(
			ioc,
			connection_settings,
			asio::dynamic_buffer(buffer_storage)
			);

	// wait for connection before sending operations
	bool is_connected{ false };
	bool notification{ false };
	std::mutex mutex;
	std::condition_variable cv;

	// the async connect will call the given completion handler
	// on error or success, we can inspect the error_code for more info
	tcp_connection->async_connect(
		[tcp_connection = tcp_connection, &is_connected, &notification, &mutex, &cv](asio::error_code ec, es::detail::tcp::tcp_package_view view)
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

		// notify one waiting thread that the connection operation has ended
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

	// read one stream event, the given completion handler will
	// be called once a server response with respect to this 
	// operation has been received or has timed out
	bool resolve_links_tos = true;
	es::async_read_stream_event(
		tcp_connection,
		stream,
		event_no,
		resolve_links_tos,
		[tcp_connection = tcp_connection, &stream, event_no = event_no](std::error_code ec, std::optional<es::event_read_result> result)
	{
		if (!ec)
		{
			auto read_result = result.value();
			ES_INFO("read event {} from stream {}", read_result.event_number(), read_result.stream());

			if (!read_result.event().has_value()) return;
			auto event = read_result.event().value();

			ES_INFO("stream-id={}\n\tresolved={}\n\tevent-number={}\n\tevent-id={}\n\tis-json={}\n\tevent-type={}\n\tcreated={}\n\tcreated_epoch={}\n\tmetadata={}\n\tdata={}",
				event.event().value().stream_id(),
				event.is_resolved(),
				event.event().value().event_number(),
				es::to_string(event.event().value().event_id()),
				event.event().value().is_json(),
				event.event().value().event_type(),
				event.event().value().created(),
				event.event().value().created_epoch(),
				event.event().value().metadata(),
				event.event().value().content()
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

	return 0;
}