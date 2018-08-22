//
// Created by daoful on 18-7-23.
//

#ifndef SRC_VOTE_H
#define SRC_VOTE_H

#include <src/lib/numbers.hpp>
#include <assert.h>
#include <src/lib/witness.h>


namespace germ
{
// Vote:
class block_vote : public std::enable_shared_from_this<germ::block_vote>
{
public:
    block_vote();
    ~block_vote();

// operations



    germ::witness    witness;
    germ::account    voter;
    germ::epoch_hash    epoch;
};

}


#endif //SRC_VOTE_H
