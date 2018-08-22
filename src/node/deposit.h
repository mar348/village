//
// Created by daoful on 18-7-27.
//

#ifndef SRC_DEPOSIT_H
#define SRC_DEPOSIT_H

#include <src/lib/numbers.hpp>


namespace germ
{


class deposit
{
public:
    deposit();
    ~deposit();


    // 申请缴纳一定量的保证金
    void pay_deposit(germ::amount const & amount);
    // 申请取消缴纳的保证金
    void cancel_deposit();

    bool deposit_paid(germ::account const & accont);
    

    germ::amount amount; // 保证金的数量

    static constexpr int deposit_warmup_period_round = 20; // 提出缴纳保证金后， 需要经过 warmup 轮投票(一轮投票包含21个epoch block)后才有资格成为见证候选人
    static constexpr int deposit_cooldown_period_epoch = 10; // 提出取消保证金后， 需要经过 cooldown个epoch block后才可以退出超级见证人列表
};

}


#endif //SRC_DEPOSIT_H
