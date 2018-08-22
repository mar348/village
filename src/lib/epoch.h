//
// Created by daoful on 18-7-23.
//

#ifndef SRC_EPOCH_H
#define SRC_EPOCH_H

#include <src/lib/numbers.hpp>
#include <blake2/blake2.h>
#include <src/lib/tx.h>
#include <src/node/utility.hpp>


namespace germ
{
enum class epoch_type : uint8_t
{
    invalid = 0,
    not_an_epoch = 1,
    epoch = 2
};

class epoch_visitor;
class epoch : public std::enable_shared_from_this<germ::epoch>
{
public:
    /* constructors */
    epoch();
    epoch(uint64_t timestamp_r,
          germ::epoch_hash const & previous_r,
          germ::signature const & signature_r,
          std::vector<epoch_hash> txs_r,
          std::vector<germ::signature> pre_votes_r,
          std::vector<germ::signature> votes_r);
    epoch (bool & error, germ::stream & stream_r);
    epoch (bool & error, boost::property_tree::ptree const & tree_r);
    //epoch (MDB_val const &);


    /* operations */
    // epoch block hash functions
    germ::epoch_hash hash() const;
    void hash (blake2b_state &) const;
    germ::epoch_hash previous_epoch () const;

    //serialize and deserialize operations
    std::string to_json ();
    void serialize (germ::stream & stream_r) const;
    void serialize_json (std::string & string) const;
    bool deserialize (germ::stream & stream_r) ;
    bool deserialize_json (boost::property_tree::ptree const & tree_r);

    // Previous block in account's chain, zero for open block
    germ::block_hash previous () const ;

    // Previous block or account number for open blocks
    bool operator== (germ::epoch const & other_r) const;
    bool operator== (germ::block const & other_r) const;

    germ::block_type type () const;

    void visit (germ::epoch_visitor & visit_r) const;
    void visit (germ::block_visitor &) const;


    /* Accessor methods */
    uint64_t                        timestamp() const;
    germ::epoch_hash                prev() const;
    std::vector<germ::block_hash>   txs() const;
    std::vector<germ::signature>    pre_votes() const;
    std::vector<germ::signature>    votes() const;
    germ::signature                 signature () const;

    /* Mutator methods*/
    void set_timestamp  (uint64_t const & timestamp_r);
    void set_prev       (germ::epoch_hash const & prev_r);
    void set_txs        (std::vector<germ::block_hash> const & txs_r);
    void set_pre_votes  (std::vector<germ::signature> const & pre_votes_r);
    void set_votes      (std::vector<germ::signature> const & votes_r);
    void set_signature  (germ::uint512_union const & signature_r);

    bool valid_predecessor (germ::epoch const & epoch_r) const;


//    static size_t constexpr size = sizeof (uint64_t)   + sizeof (germ::epoch_hash) +
//                                   sizeof (epoch_hash) + sizeof (germ::signature);


    size_t size() const;
    //germ::mdb_val val () const;

private:
    uint64_t timestamp_;
    germ::epoch_hash prev_;
    std::vector<germ::block_hash> txs_;
    std::vector<germ::signature> pre_votes_;   // size: 15 ~ 20
    std::vector<germ::signature> votes_;       // size: 15 ~ 20
    germ::signature signature_;
};


class epoch_message : public epoch
{
public:
    epoch_message();
    epoch_message(germ::epoch_hash const & epoch_r);
    epoch_message(uint64_t timestamp_r, germ::epoch_hash const & previous_r,
                  germ::signature const & signature_r,
                  germ::epoch_hash const & epoch_r, std::vector<germ::block_hash> txs_r,
                  std::vector<germ::signature> pre_votes_r, std::vector<germ::signature> votes_r);
    /* Accessor and Mutator */
    germ::epoch_hash epoch_hash() const;
    void set_epoch_hash(uint64_t const & epoch_hash_r);

private:
    germ::epoch_hash epoch_hash_;
};

class epoch_visitor
{
public:
    virtual void epoch_block (germ::epoch const & epoch_r) = 0;
    virtual ~epoch_visitor () = default;
};


template <typename T>
bool epoch_equal (T const & first, germ::epoch const & second)
{
    static_assert (std::is_base_of<germ::epoch, T>::value, "Input parameter is not a epoch type");
    return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}


std::unique_ptr<germ::epoch> deserialize_epoch (germ::stream & stream_r);
std::unique_ptr<germ::epoch> deserialize_epoch (germ::stream & stream_r, germ::block_type type_r);
std::unique_ptr<germ::epoch> deserialize_epoch_json (boost::property_tree::ptree const & tree_r);
void serialize_epoch (germ::stream & stream_r, germ::epoch const & epoch_r);
}


#endif //SRC_EPOCH_H
