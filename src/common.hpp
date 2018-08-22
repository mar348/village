#pragma once

#include <src/lib/blocks.hpp>
#include <src/node/utility.hpp>

#include <boost/property_tree/ptree.hpp>

#include <unordered_map>

#include <blake2/blake2.h>
#include <src/lib/tx.h>

#define GERM_STD (0)

namespace boost
{
template <>
struct hash<germ::uint256_union>
{
    size_t operator() (germ::uint256_union const & value_a) const
    {
        std::hash<germ::uint256_union> hash;
        return hash (value_a);
    }
};
}
namespace germ
{
const uint8_t protocol_version = 0x0c;
const uint8_t protocol_version_min = 0x07;
const uint8_t node_id_version = 0x0c;

class block_store;
/**
 * Determine the balance as of this block
 */
class balance_visitor : public germ::block_visitor
{
public:
    balance_visitor (MDB_txn *, germ::block_store &);
    virtual ~balance_visitor () = default;
    void compute (germ::block_hash const &);
    void send_block (germ::send_block const &) override;
    void receive_block (germ::receive_block const &) override;
    void open_block (germ::open_block const &) override;
    void change_block (germ::change_block const &) override;
    void state_block (germ::state_block const &) override;
    void tx (germ::tx const & tx) override;
    MDB_txn * transaction;
    germ::block_store & store;
    germ::block_hash current_balance;
    germ::block_hash current_amount;
    germ::uint128_t balance;
};

/**
 * Determine the amount delta resultant from this block
 */
class amount_visitor : public germ::block_visitor
{
public:
    amount_visitor (MDB_txn *, germ::block_store &);
    virtual ~amount_visitor () = default;
    void compute (germ::block_hash const &);
    void send_block (germ::send_block const &) override;
    void receive_block (germ::receive_block const &) override;
    void open_block (germ::open_block const &) override;
    void change_block (germ::change_block const &) override;
    void state_block (germ::state_block const &) override;
    void from_send (germ::block_hash const &);
    void tx(germ::tx const& tx) override;
    MDB_txn * transaction;
    germ::block_store & store;
    germ::block_hash current_amount;
    germ::block_hash current_balance;
    germ::uint128_t amount;
};

/**
 * Determine the representative for this block
 */
class representative_visitor : public germ::block_visitor
{
public:
    representative_visitor (MDB_txn * transaction_a, germ::block_store & store_a);
    virtual ~representative_visitor () = default;
    void compute (germ::block_hash const & hash_a);
    void send_block (germ::send_block const & block_a) override;
    void receive_block (germ::receive_block const & block_a) override;
    void open_block (germ::open_block const & block_a) override;
    void change_block (germ::change_block const & block_a) override;
    void state_block (germ::state_block const & block_a) override;
    void tx(germ::tx const& tx) override;
    MDB_txn * transaction;
    germ::block_store & store;
    germ::block_hash current;
    germ::block_hash result;
};

/**
 * A key pair. The private key is generated from the random pool, or passed in
 * as a hex string. The public key is derived using ed25519.
 */
class keypair
{
public:
    keypair ();
    keypair (std::string const &);
    keypair (germ::raw_key &&);
    germ::public_key pub;
    germ::raw_key prv;
};

std::unique_ptr<germ::tx> deserialize_block (MDB_val const &);

/**
 * Latest information about an account
 */
class account_info
{
public:
    account_info ();
    account_info (MDB_val const &);
    account_info (germ::account_info const &) = default;
    account_info (germ::block_hash const &, /*germ::block_hash const &,*/ germ::block_hash const &, germ::amount const &, uint64_t, uint64_t);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    bool operator== (germ::account_info const &) const;
    bool operator!= (germ::account_info const &) const;
    germ::mdb_val val () const;
    germ::block_hash head;
//    germ::block_hash rep_block;
    germ::block_hash open_block;
    germ::amount balance;
    /** Seconds since posix epoch */
    uint64_t modified;
    uint64_t block_count;
};

/**
 * Information on an uncollected send
 */
class pending_info
{
public:
    pending_info ();
    pending_info (MDB_val const &);
    pending_info (germ::account const &, germ::amount const &);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    bool operator== (germ::pending_info const &) const;
    germ::mdb_val val () const;
    germ::account source;
    germ::amount amount;
};
class pending_key
{
public:
    pending_key (germ::account const &, germ::block_hash const &);
    pending_key (MDB_val const &);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    bool operator== (germ::pending_key const &) const;
    germ::mdb_val val () const;
    germ::account account;
    germ::block_hash hash;
};
class block_info
{
public:
    block_info ();
    block_info (MDB_val const &);
    block_info (germ::account const &, germ::amount const &);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    bool operator== (germ::block_info const &) const;
    germ::mdb_val val () const;
    germ::account account;
    germ::amount balance;
};
class block_counts
{
public:
    block_counts ();
    size_t sum ();
    size_t send;
    size_t receive;
    size_t open;
    size_t change;
    size_t state;
};

/**
 * Latest information about an epoch block
 */
class epoch_info
{
public:
    epoch_info ();
    epoch_info (MDB_val const &);
    epoch_info (germ::epoch_info const &) = default;
    epoch_info (germ::block_hash const &, uint64_t, uint64_t);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    bool operator== (germ::epoch_info const &) const;
    bool operator!= (germ::epoch_info const &) const;
    germ::mdb_val val () const;
    germ::epoch_hash head;
    /** Seconds since posix epoch */
    uint64_t modified;
    uint64_t block_count;
};

class vote
{
public:
    vote () = default;
    vote (germ::vote const &);
    vote (bool &, germ::stream &);
    vote (bool &, germ::stream &, germ::block_type);
    vote (germ::account const &, germ::raw_key const &, uint64_t, std::shared_ptr<germ::tx>);
    vote (MDB_val const &);
    germ::uint256_union hash () const;
    bool operator== (germ::vote const &) const;
    bool operator!= (germ::vote const &) const;
    void serialize (germ::stream &, germ::block_type);
    void serialize (germ::stream &);
    bool deserialize (germ::stream &);
    bool validate ();
    std::string to_json () const;
    // Vote round sequence number
    uint64_t sequence;
    std::shared_ptr<germ::tx> block;
    // Account that's voting
    germ::account account;
    // Signature of sequence + block hash
    germ::signature signature;
};
enum class vote_code
{
    invalid, // Vote is not signed correctly
    replay, // Vote does not have the highest sequence number, it's a replay
    vote // Vote has the highest sequence number
};

enum class process_result
{
    progress, // Hasn't been seen before, signed correctly
    bad_signature, // Signature was bad, forged or transmission error
    old, // Already seen and was valid
    negative_spend, // Malicious attempt to spend a negative amount
    fork, // Malicious fork based on previous
    unreceivable, // Source block doesn't exist or has already been received
    gap_previous, // Block marked as previous is unknown
    gap_source, // Block marked as source is unknown
    opened_burn_account, // The impossible happened, someone found the private key associated with the public key '0'.
    balance_mismatch, // Balance and amount delta don't match
    block_position // This block cannot follow the previous block
};
class process_return
{
public:
    germ::process_result code;
    germ::account account;
    germ::amount amount;
    germ::account pending_account;
    boost::optional<bool> state_is_send;
};
enum class tally_result
{
    vote,
    changed,
    confirm
};
class votes
{
public:
    votes (std::shared_ptr<germ::tx>);
    germ::tally_result vote (std::shared_ptr<germ::vote>);
    bool uncontested ();
    // Root block of fork
    germ::block_hash id;
    // All votes received by account
    std::unordered_map<germ::account, std::shared_ptr<germ::tx>> rep_votes;
};
extern germ::keypair const & zero_key;
extern germ::keypair const & test_genesis_key;
extern germ::account const & rai_test_account;
extern germ::account const & rai_beta_account;
extern germ::account const & rai_live_account;
extern std::string const & rai_test_genesis;
extern std::string const & rai_beta_genesis;
extern std::string const & rai_live_genesis;
extern std::string const & genesis_block;
extern germ::account const & genesis_account;
extern germ::account const & burn_account;
extern germ::uint128_t const & genesis_amount;
// A block hash that compares inequal to any real block hash
extern germ::block_hash const & not_a_block;
// An account number that compares inequal to any real account number
extern germ::block_hash const & not_an_account;
class genesis
{
public:
    explicit genesis ();
    void initialize (MDB_txn *, germ::block_store &) const;
    germ::block_hash hash () const;
//    std::unique_ptr<germ::open_block> open;
    std::unique_ptr<germ::tx> open;
};
}
