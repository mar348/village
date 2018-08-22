//
// Created by daoful on 18-7-23.
//

#ifndef SRC_SMART_CONTRACT_STATE_H
#define SRC_SMART_CONTRACT_STATE_H

#include <src/lib/numbers.hpp>

namespace germ
{
// Smart contract state:
class smart_contract_state
{
public:
    smart_contract_state();
    ~smart_contract_state();

// operations


private:
    std::string    output;
    std::string    logs;
};

}


#endif //SRC_SMART_CONTRACT_STATE_H
