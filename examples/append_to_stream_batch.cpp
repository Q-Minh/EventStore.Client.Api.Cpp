#include <mutex>
#include <condition_variable>

#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include "event_data.hpp"
#include "append_to_stream.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 7)
	{
		ES_ERROR("expected 6 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> <num-events> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./append-to-stream 127.0.0.1 1113 admin changeit 1000 info");
		ES_ERROR("tool will write the following data to event store");
		ES_ERROR("stream={}\n\tevent-type={}\n\tis-json={}\n\tdata={}\n\tmetadata={}", "test-stream", "Test.Type", true, "{ \"test\": \"data\"}", "test metadata");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	int num_events = std::stoi(argv[5]);
	std::string_view lvl = argv[6];
	ES_DEFAULT_LOG_LEVEL(lvl);

	boost::asio::io_context ioc;

	boost::asio::ip::tcp::endpoint endpoint;
	endpoint.address(boost::asio::ip::make_address_v4(ep));
	endpoint.port(port);

	// all operation will be authenticated by default
	es::user::user_credentials credentials(username, password);

	auto connection_settings =
		es::connection_settings_builder()
		.with_default_user_credentials(credentials)
		.require_master(false)
		.build();

	// register the discovery service to enable the es tcp connection to discover endpoints to connect to
	// for now, the discovery service doesn't do anything, since we haven't implemented cluster node discovery
	using discovery_service_type = es::tcp::services::discovery_service;
	auto& discovery_service = boost::asio::make_service<discovery_service_type>(ioc, endpoint, boost::asio::ip::tcp::endpoint(), false);

	// parameterize our tcp connection with steady timer, the basic discovery service and our type-erased operation
	using connection_type =
		es::connection::basic_tcp_connection<
		boost::asio::steady_timer,
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
			boost::asio::dynamic_buffer(buffer_storage)
			);

	// wait for connection before sending operations
	bool is_connected{ false };
	bool notification{ false };
	std::mutex mutex;
	std::condition_variable cv;

	// the async connect will call the given completion handler
	// on error or success, we can inspect the error_code for more info
	tcp_connection->async_connect(
		[tcp_connection = tcp_connection, &is_connected, &notification, &mutex, &cv](boost::system::error_code ec, std::optional<es::connection_result> result)
	{
		std::lock_guard<std::mutex> lock(mutex);
		notification = true;

		if (!ec || ec == es::connection_errors::authentication_failed)
		{
			ES_INFO("client has been identified with connection-id={}, {}",
				es::to_string(result.value().connection_id()),
				ec.message()
			);
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

	std::vector<es::event_data> events;
	events.reserve(num_events);

	ES_INFO("sending events to es");
	for (int i = 0; i < num_events; ++i)
	{
		es::event_data event{ es::guid(), "Test.Type", true, "{ \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\", \"test\": \"data\"}", "test metadata" };
		events.push_back(event);
	}
	std::string stream_name = "test-stream";

	// append one stream event, the given completion handler will
	// be called once a server response with respect to this 
	// operation has been received or has timed out
	auto begin = std::chrono::high_resolution_clock::now();

	es::async_append_to_stream(
		tcp_connection,
		stream_name,
		std::move(events),
		[tcp_connection = tcp_connection, stream_name, begin = begin](boost::system::error_code ec, std::optional<es::write_result> result)
	{
		if (!ec)
		{
			auto end = std::chrono::high_resolution_clock::now();
			ES_INFO("successfully appended to stream {}, next expected version={}, in {} ms", 
				stream_name, 
				result.value().next_expected_version(), 
				ES_MILLISECONDS(end - begin));
		}
		else
		{
			ES_ERROR("error appending to stream : {}", ec.message());
			return;
		}
	}
	);

	message_pump.join();

	return 0;
}