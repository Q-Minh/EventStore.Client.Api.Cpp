#include <mutex>
#include <condition_variable>

#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/address.hpp>

#include "event_data.hpp"
#include "append_to_stream.hpp"

#include "connection/basic_tcp_connection.hpp"
#include "tcp/discovery_service.hpp"
#include "tcp/cluster_discovery_service.hpp"

int main(int argc, char** argv)
{
	GOOGLE_PROTOBUF_VERSION;

	if (argc != 10)
	{
		ES_ERROR("expected 9 arguments, got {}", argc - 1);
		ES_ERROR("usage: <executable> <username> <password> <gossip-seed1-ip> <port> <gossip-seed2-ip> <port> <gossip-seed3-ip> <port> [trace | debug | info | warn | error | critical | off]");
		ES_ERROR("example: ./append-to-stream-cluster admin changeit 127.0.0.1 3112 127.0.0.1 4112 127.0.0.1 5112 info");
		ES_ERROR("tool will write the following data to event store");
		ES_ERROR("stream={}\n\tevent-type={}\n\tis-json={}\n\tdata={}\n\tmetadata={}", "test-stream", "Test.Type", true, "{ \"test\": \"data\"}", "test metadata");
		return 0;
	}

	// get command arguments
	std::string username = argv[1];
	std::string password = argv[2];
	std::string ep1 = argv[3], ep2 = argv[5], ep3 = argv[7];
	int port1 = std::stoi(argv[4]), port2 = std::stoi(argv[6]), port3 = std::stoi(argv[8]);
	std::string_view lvl = argv[9];
	ES_DEFAULT_LOG_LEVEL(lvl);

	boost::asio::io_context ioc;

	// all operation will be authenticated by default
	es::user::user_credentials credentials(username, password);

	auto connection_settings =
		es::connection_settings_builder()
		.with_default_user_credentials(credentials)
		.require_master(false)
		.with_gossip_seeds({ 
			boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address_v4(ep1), port1),
			boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address_v4(ep2), port2),
			boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address_v4(ep3), port3)
		})
		.build();

	// register the discovery service to enable the es tcp connection to discover endpoints to connect to
	// for now, the discovery service doesn't do anything, since we haven't implemented cluster node discovery
	using discovery_service_type = es::tcp::services::cluster_discovery_service;

	auto& discovery_service = boost::asio::make_service<discovery_service_type>(
		ioc, 
		es::cluster_settings_builder().keep_discovering()
		.with_gossip_seed_endpoints(
			connection_settings.gossip_seeds().begin(), 
			connection_settings.gossip_seeds().end()
		)
		.with_max_discover_attempts(10)
		.with_gossip_timeout(std::chrono::seconds(60))
		.build()
	);

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
	es::event_data event{ es::guid(), "Test.Type", true, "{ \"test\": \"data\"}", "test metadata" };
	events.push_back(event);
	std::string stream_name = "test-stream";

	// append one stream event, the given completion handler will
	// be called once a server response with respect to this 
	// operation has been received or has timed out
	es::async_append_to_stream(
		tcp_connection,
		stream_name,
		std::move(events),
		[tcp_connection = tcp_connection, stream_name](boost::system::error_code ec, std::optional<es::write_result> result)
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

	message_pump.join();

	return 0;
}