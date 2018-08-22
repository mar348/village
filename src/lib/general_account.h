//
// Created by daoful on 18-8-1.
//

#ifndef SRC_GENERAL_ACCOUNT_H
#define SRC_GENERAL_ACCOUNT_H

#include <src/common.hpp>
#include <src/node/deposit.h>

namespace germ
{

class lag
{
public:
    germ::amount amount;
    germ::account destination;
    germ::account source;
    uint64_t epoch_count;  // remaining epoch count
};

class general_account
{
public:
    general_account();
    general_account(germ::account const & account_r, germ::amount const & deposit_r);


    germ::account account;
    germ::amount deposit; // 保证金的数量
//    std::vector<lag> lags; // 滯後的保證金

};

}


#endif //SRC_GENERAL_ACCOUNT_H
