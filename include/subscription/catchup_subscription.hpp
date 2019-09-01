#pragma once

#ifndef ES_CATCHUP_SUBSCRIPTION_HPP
#define ES_CATCHUP_SUBSCRIPTION_HPP

#include "subscription_base.hpp"

namespace es {
namespace subscription {

template <class ConnectionType>
class catchup_subscription
	: public subscription_base<ConnectionType, catchup_subscription<ConnectionType>>
{
public:
	using connection_type = ConnectionType;
	using operations_map_type = typename connection_type::operations_map_type;
	using op_key_type = typename operations_map_type::key_type;
	using self_type = catchup_subscription<connection_type>;
	using base_type = subscription_base<connection_type, self_type>;

private:

};

}
}

#endif // ES_CATCHUP_SUBSCRIPTION_HPP