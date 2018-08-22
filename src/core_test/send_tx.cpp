//
// Created by daoful on 18-7-26.
//

#include <boost/property_tree/json_parser.hpp>

#include <fstream>

#include <gtest/gtest.h>

#include <src/lib/interface.h>
#include <src/node/common.hpp>
#include <src/node/node.hpp>

#include <ed25519-donna/ed25519.h>

TEST (send_tx, send)
{
    std::cout << "send" << std::endl;
}