//
// Created by daoful on 18-7-27.
//

#ifndef SRC_WITNESS_CANDIDATE_H
#define SRC_WITNESS_CANDIDATE_H


#include <src/lib/numbers.hpp>
#include <assert.h>
#include <boost/property_tree/json_parser.hpp>
#include <src/node/deposit.h>
#include <src/lib/general_account.h>


namespace germ
{

class election;
class witness_candidate : public germ::general_account
{
public:
    witness_candidate();
    ~witness_candidate();


    std::shared_ptr<germ::election> election;
//    germ::account account;
    germ::epoch_hash epoch;
    germ::account voter;
    germ::amount amount;
//    germ::deposit deposit;


    static constexpr int witness_candidate_count = 50;   // 前50名候选见证人
};

}


#endif //SRC_WITNESS_CANDIDATE_H
