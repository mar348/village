//
// Created by daoful on 18-7-23.
//

#ifndef SRC_ELECTION_H
#define SRC_ELECTION_H

#include <src/lib/numbers.hpp>
#include <assert.h>
#include <src/lib/witness.h>
#include <boost/multi_index/hashed_index.hpp>

namespace germ
{

class election_tally
{
public:

//    election_tally(germ::amount const & weight_r, germ::account const & account_r);

    germ::account account;
    germ::amount weight;   // 账户与权重的对应
};
    
// Election:
class account_election : public std::enable_shared_from_this<germ::account_election>
{
public:
    account_election();
    ~account_election();

    // elect a candidate
    bool elect_candidate();
    std::shared_ptr<germ::account_election> shared();



    germ::epoch_hash epoch;

    
};

}


#endif //SRC_ELECTION_H
