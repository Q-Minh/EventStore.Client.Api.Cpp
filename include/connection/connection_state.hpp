#pragma once

#ifndef ES_CONNECTION_STATE_HPP
#define ES_CONNECTION_STATE_HPP

namespace es {
namespace internal {

enum class connection_state
{
	init,
	connecting,
	connected,
	closed
};

}
}

#endif // ES_CONNECTION_STATE_HPP