//
// Created by daoful on 18-7-23.
//

#ifndef SRC_WITNESS_H
#define SRC_WITNESS_H


#include <src/lib/witness_candidate.h>

namespace germ
{

// Witness:
class witness : public germ::witness_candidate
{
public:
    witness();
    ~witness();

// operations
// 成为witness candidate的条件：
// 1. 权重排名在前50名
// 2. 缴纳一定保证金
// 3. 必须经过m个epoch block
    bool    mature_validate();

// 评分规则：
// 1. 没有作恶， 评分会随着epoch数量线性增长
// 2. 如果轮到该witness投票，但是未投， 会减分
// 3. 如果在分叉时，在两条链上都进行了投票， 会减分
    void    rating();

// witness申请撤销保证金条件：
// 1. 必须经过m个epoch才可以撤销
    void    request_withdraw();

// 申请抵押保证金条件：
// 1. 保证金达到一定额度
// 2. 必须经过m个epoch block才算抵押成功
    void    request_impawn();


    static constexpr int witness_count = 21;   // 前21名超级见证人
};

}


#endif //SRC_WITNESS_H
