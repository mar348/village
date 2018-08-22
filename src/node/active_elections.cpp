//
// Created by daoful on 18-8-2.
//

#include <src/node/active_elections.h>
#include <src/node/node.hpp>


constexpr int germ::active_elections::deposit_warmup_period_round;
constexpr int germ::active_elections::deposit_cooldown_period_epoch;
constexpr int germ::active_elections::witness_max_count_per_voting;

germ::active_elections::active_elections(germ::node & node_r)
:node(node_r)
{
}

germ::active_elections::~active_elections()
{
}


bool germ::active_elections::vote(germ::tx & block, germ::general_account const & general)
{
    std::lock_guard<std::mutex> lock (mutex);

    auto deposited(false);
    for (auto i (lists.begin ()), n (lists.end ()); i != n; ++i)
    {
        if (general.account == i->account)
        {
            deposited = true;
            break;
        }
    }

    if ( !deposited )
        return false;

    // deposit paid
    germ::block_hash hash = block.hash();
    germ::transaction transaction_r(node.store.environment, nullptr, false);
    germ::amount balance = node.store.block_balance(transaction_r, hash);
    auto account(lists.find(general.account));
    if (account != lists.end())
    {
        lists.modify(account, [&balance](germ::election_tally& election){
            germ::uint128_t weight = election.weight.number() + balance.number();
            election.weight = germ::amount(weight);
        });
    }
    else
    {
        lists.insert(germ::election_tally{ general.account, balance });
    }


    return true;
}

bool germ::active_elections::witness_account()
{
    auto result(false);

    germ::transaction transaction_a (node.store.environment, nullptr, false);
    for (auto beg(node.wallets.items.begin()), end(node.wallets.items.end()); beg != end; ++beg)
    {
        auto wallet(beg->second);
        for (auto acc_beg (wallet->store.begin (transaction_a)), acc_end (wallet->store.end ()); acc_beg != acc_end; ++acc_beg)
        {
            germ::account account(germ::uint256_union (acc_beg->first.uint256 ()));
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (auto i(witnesses.begin()), n(witnesses.end()); i != n; ++i )
                {
                    if( i->account == account )
                        return true;
                }
            }
        }
    }

    return result;
}

void germ::active_elections::pay_deposit(germ::account const & account_r, germ::amount const & amount_r)
{
    
}

void germ::active_elections::cancel_deposit(germ::account  const & account_r)
{

}

bool germ::active_elections::deposited(germ::account const &account)
{
    auto result(false);




    result = true; // default;
    return result;
}
