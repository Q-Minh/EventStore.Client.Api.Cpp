#pragma once

#ifndef ES_MOCK_TCP_CONNECTION_HPP
#define ES_MOCK_TCP_CONNECTION_HPP

namespace es {
namespace test {

/*
	For now, the only type requirements for ConnectionType for read operations
	are these
*/
template <class AsyncReadStream>
class mock_tcp_connection
{
public:
	explicit mock_tcp_connection(AsyncReadStream&& read_stream)
		: read_stream_(std::move(read_stream))
	{}

	AsyncReadStream& socket() { return read_stream_; }
private:
	AsyncReadStream read_stream_;
};

}
}

#endif // ES_MOCK_TCP_CONNECTION_HPP