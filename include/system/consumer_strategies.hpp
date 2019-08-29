#pragma once

#ifndef ES_CONSUMER_STRATEGIES_HPP
#define ES_CONSUMER_STRATEGIES_HPP

namespace es {
namespace system {

struct consumer_strategies
{
	inline static const char dispatch_to_single[] = "DispatchToSingle";
	inline static const char round_robin[] = "RoundRobin";
	inline static const char pinned[] = "Pinned";
};

}
}

#endif // ES_CONSUMER_STRATEGIES_HPP