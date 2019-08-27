#pragma once

#ifndef ES_CONNECTION_PHASE_HPP
#define ES_CONNECTION_PHASE_HPP

namespace es {
namespace internal {

enum class connection_phase
{
	invalid,
	reconnecting,
	end_point_discovery,
	connection_establishing,
	authentication,
	identification,
	connected
};

}
}

#endif // ES_CONNECTION_PHASE_HPP