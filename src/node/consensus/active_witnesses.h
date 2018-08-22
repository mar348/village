//
// Created by daoful on 18-7-27.
//

#ifndef SRC_ACTIVE_WITNESSES_H
#define SRC_ACTIVE_WITNESSES_H


#include <src/node/node.hpp>

namespace germ
{


class active_witnesses
{
public:
    active_witnesses(germ::node& node_r);



    germ::node& node;
};

}


#endif //SRC_ACTIVE_WITNESSES_H
