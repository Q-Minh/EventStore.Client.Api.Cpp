#include <mutex>
#include <condition_variable>

#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include "connection/basic_tcp_connection.hpp"
#include "expected_version.hpp"
#include "get_stream_metadata.hpp"
#include "set_stream_metadata.hpp"
#include "tcp/discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 6)
	{
		ES_ERROR("expected 5 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <ip endpoint> <port> <username> <password> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./stream-metadata 127.0.0.1 1113 admin changeit info");
		ES_ERROR("tool will write the following metadata to the stream 'test-stream' and fetch it back: {}", "{ \"key\": 1029014814 }");
		return 0;
	}

	// get command arguments
	std::string ep = argv[1];
	int port = std::stoi(argv[2]);
	std::string username = argv[3];
	std::string password = argv[4];
	std::string_view lvl = argv[5];
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

	auto tcp_connection = std::make_shared<connection_type>(ioc, connection_settings);

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

	std::string stream = "test-stream";
	std::string metadata = "{ \"key\": 1029014814 }";

	auto get_stream_metadata_handler = 
		[&stream](boost::system::error_code ec, std::optional<es::stream_metadata_result_raw> result)
	{
		if (!ec)
		{
			ES_INFO("fetched stream metata from stream={}", stream);
			ES_INFO("is-stream-deleted={}, meta-stream-version={}, stream={}", 
				result.value().is_stream_deleted(), 
				result.value().meta_stream_version(), 
				result.value().stream()
			);
			ES_INFO("stream-metadata={}", result.value().stream_metadata());
			return;
		}
		else
		{
			ES_ERROR("failed to fetch stream metadata from stream={}", stream);
			return;
		}
	};

	auto set_stream_metadata_handler = 
		[&stream, handler = std::move(get_stream_metadata_handler), connection=tcp_connection](boost::system::error_code ec, std::optional<es::write_result> result)
	{
		if (!ec)
		{
			ES_INFO("wrote stream metadata, next-expected-version={}", result.value().next_expected_version());
			es::async_get_stream_metadata(
				connection,
				stream,
				std::move(handler)
			);
			return;
		}
		else
		{
			ES_ERROR("failed to set stream metadata for stream={}, {}", stream, ec.message());
			return;
		}
	};

	es::async_set_stream_metadata(
		tcp_connection,
		stream,
		(std::int64_t)es::expected_version::any,
		metadata,
		std::move(set_stream_metadata_handler)
	);

	message_pump.join();

	return 0;
}