#pragma once

#include <chrono>
#include <cstddef>

namespace germ
{
// Network variants with different genesis blocks and network parameters
enum class germ_networks
{
    // Low work parameters, publicly known genesis key, test IP ports
    germ_test_network,
    // Normal work parameters, secret beta genesis key, beta IP ports
    germ_beta_network,
    // Normal work parameters, secret live key, live IP ports
    germ_live_network
};
germ::germ_networks const rai_network = germ_networks::ACTIVE_NETWORK;
std::chrono::milliseconds const transaction_timeout = std::chrono::milliseconds (1000);
}
