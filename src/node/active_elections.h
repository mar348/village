//
// Created by daoful on 18-8-2.
//

#ifndef SRC_ACTIVE_ELECTIONS_H
#define SRC_ACTIVE_ELECTIONS_H

#include <thread>
#include <src/lib/election.h>
#include <src/lib/tx.h>

namespace germ
{

class node;
class active_elections
{
public:
    active_elections(germ::node & node_r);
    ~active_elections();

    bool vote(germ::tx & block, germ::general_account const & general);
    bool witness_account();


    // 申请缴纳一定量的保证金
    void pay_deposit(germ::account const & account_r, germ::amount const & amount_r);
    // 申请取消缴纳的保证金
    void cancel_deposit(germ::account  const & account_r);

    bool deposited(germ::account const & account);


    germ::node& node;
    std::mutex mutex;


    std::array<germ::witness_candidate, germ::witness_candidate::witness_candidate_count> witness_candidates;  // 前50名候选见证人列表
    std::array<germ::witness, germ::witness::witness_count> witnesses;  // 前21名超级见证人列表

    boost::multi_index_container<
    germ::election_tally,
    boost::multi_index::indexed_by<
            boost::multi_index::hashed_unique<boost::multi_index::member<germ::election_tally, germ::account, &germ::election_tally::account>>,
            boost::multi_index::ordered_non_unique<boost::multi_index::member<germ::election_tally, germ::amount , &germ::election_tally::weight>>>>
    lists;  //


    static constexpr int deposit_warmup_period_round = 20; // 提出缴纳保证金后， 需要经过 warmup 轮投票(一轮投票包含21个epoch block)后才有资格成为见证候选人
    static constexpr int deposit_cooldown_period_epoch = 10; // 提出取消保证金后， 需要经过 cooldown个epoch block后才可以退出超级见证人列表
    static constexpr int witness_max_count_per_voting = 22; // 每个账户可以最多选举22个超级节点
};


}


#endif //SRC_ACTIVE_ELECTIONS_H
