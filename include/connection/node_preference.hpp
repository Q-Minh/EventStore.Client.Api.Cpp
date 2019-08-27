#pragma once

#ifndef NODE_PREFERENCE_HPP
#define NODE_PREFERENCE_HPP

// put in es::connection namespace ??
namespace es {

enum class node_preference
{
    master,
    slave,
    random
};

}

#endif