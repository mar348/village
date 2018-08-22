//
// Created by daoful on 18-7-23.
//

#ifndef SRC_SMART_CONTRACT_H
#define SRC_SMART_CONTRACT_H


#include <src/lib/numbers.hpp>
#include <assert.h>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <src/lib/blocks.hpp>


namespace germ
{
enum class contract_type : uint8_t
{
    invalid = 0,
    not_a_contract = 1,
    election = 2,
    epoch = 3,
    witness = 4,
    regular = 5    // 
};

class smart_contract : public std::enable_shared_from_this<germ::smart_contract>
{
public:
    smart_contract (germ::account const & address, std::string const & bytecode);
    smart_contract (bool & error, germ::stream & stream_r);
    smart_contract (bool & error, boost::property_tree::ptree const & tree_r);


    void serialize (germ::stream & stream_r) const;
    void serialize_json (std::string & string_r) const;
    bool deserialize (germ::stream & stream_r) ;
    bool deserialize_json (boost::property_tree::ptree const & tree_r);
    bool operator== (germ::smart_contract const & other_r) const;
    germ::contract_type type () const;
    bool valid_predecessor (germ::smart_contract const & smart_contract) const;
    

    germ::account    contract_address;
    std::string    contract_bytecode;
};

}


#endif //SRC_SMART_CONTRACT_H
