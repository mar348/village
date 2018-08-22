//
// Created by daoful on 18-7-23.
//

#ifndef SRC_TX_H
#define SRC_TX_H

#include <src/lib/numbers.hpp>
#include <assert.h>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <src/lib/blocks.hpp>


namespace germ
{

class tx_message
{
public:
    tx_message();
    tx_message(germ::tx_message const &tx_info_r);
    tx_message(uint64_t const & tx_value, std::string const & tx_data, uint64_t const & tx_gas, uint64_t const & tx_gas_price);
    uint64_t    value;          //
    std::string    data;
    uint64_t    gas;
    uint64_t    gas_price;
};

// Transaction: 
class tx
{
public:
    tx (germ::block_hash const & previous_r, germ::account const & destination_r, germ::block_hash source_r, germ::account const & account, germ::amount const & balance_r, germ::tx_message const &tx_info, germ::epoch_hash const & epoch_r, germ::raw_key const & prv_r, germ::public_key const & pub_r);
    tx (bool & error, germ::stream & stream);
    tx (bool & error, boost::property_tree::ptree const & tree);
    ~tx ();

    germ::block_hash hash () const;
    std::string to_json ();


    void hash (blake2b_state & hash_r) const ;
    germ::account destination() const ;
    germ::block_hash previous () const ;
    germ::block_hash source () const ;
    germ::block_hash root () const ;
    void serialize (germ::stream &) const ;
    void serialize_json (std::string &) const ;
    bool deserialize (germ::stream & stream_r) ;
    bool deserialize_json (boost::property_tree::ptree const & tree_r);
    bool operator== (germ::tx const & other) const;
    void visit (germ::block_visitor & visit_r) const  ;
    germ::block_type type () const ;
    germ::signature block_signature () const ;
    void signature_set (germ::uint512_union const & signature_r) ;
    bool valid_predecessor (germ::tx const & tx) const ;

    static size_t constexpr size_ = sizeof (germ::account) + sizeof (germ::block_hash) + sizeof (germ::block_hash) + sizeof (germ::amount) + sizeof(germ::account) +
                                   sizeof (uint64_t) + sizeof (germ::signature) + sizeof (uint64_t) + sizeof (uint64_t) +  sizeof (std::string) +
                                   sizeof (epoch_hash) +
                                   sizeof (germ::signature) + sizeof (uint64_t);

    size_t size() const;


    germ::block_hash previous_;
    germ::account destination_;
    germ::block_hash source_;
    germ::amount balance_;
    germ::account account_;

//    std::vector<germ::amount> accounts_voted;  //  被选举的账户（必须已缴纳保证金）

    germ::tx_message   tx_info;

    germ::epoch_hash    epoch;

    germ::signature signature;
};

template <typename T>
bool txs_equal (T const & first, germ::tx const & second)
{
    static_assert (std::is_base_of<germ::tx, T>::value, "Input parameter is not a block type");
    return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}

template <typename T>
bool blocks_equal (T const & first, germ::block const & second)
{
    static_assert (std::is_base_of<germ::block, T>::value, "Input parameter is not a block type");
    return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}

}


#endif //SRC_TX_H
