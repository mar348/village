#pragma once

#include <src/common.hpp>

namespace germ
{
/**
 * Iterates the key/value pairs of a transaction
 */
class store_iterator
{
public:
    store_iterator (MDB_txn *, MDB_dbi);
    store_iterator (std::nullptr_t);
    store_iterator (MDB_txn *, MDB_dbi, MDB_val const &);
    store_iterator (germ::store_iterator &&);
    store_iterator (germ::store_iterator const &) = delete;
    ~store_iterator ();
    germ::store_iterator & operator++ ();
    void next_dup ();
    germ::store_iterator & operator= (germ::store_iterator &&);
    germ::store_iterator & operator= (germ::store_iterator const &) = delete;
    std::pair<germ::mdb_val, germ::mdb_val> * operator-> ();
    bool operator== (germ::store_iterator const &) const;
    bool operator!= (germ::store_iterator const &) const;
    void clear ();
    MDB_cursor * cursor;
    std::pair<germ::mdb_val, germ::mdb_val> current;
};

/**
 * Manages block storage and iteration
 */
class block_store
{
public:
    block_store (bool &, boost::filesystem::path const &, int lmdb_max_dbs = 128);

    MDB_dbi block_database (germ::block_type);
    void block_put_raw (MDB_txn *, MDB_dbi, germ::block_hash const &, MDB_val);
    void block_put (MDB_txn *, germ::block_hash const &, germ::tx const &, germ::block_hash const & = germ::block_hash (0));
    MDB_val block_get_raw (MDB_txn *, germ::block_hash const &, germ::block_type &);
    germ::block_hash block_successor (MDB_txn *, germ::block_hash const &);
    void block_successor_clear (MDB_txn *, germ::block_hash const &);
    std::unique_ptr<germ::tx> block_get (MDB_txn *, germ::block_hash const &);
    std::unique_ptr<germ::tx> block_random (MDB_txn *);
    std::unique_ptr<germ::tx> block_random (MDB_txn *, MDB_dbi);
    void block_del (MDB_txn *, germ::block_hash const &);
    bool block_exists (MDB_txn *, germ::block_hash const &);
    germ::block_counts block_count (MDB_txn *);
    bool root_exists (MDB_txn *, germ::uint256_union const &);

    void frontier_put (MDB_txn *, germ::block_hash const &, germ::account const &);
    germ::account frontier_get (MDB_txn *, germ::block_hash const &);
    void frontier_del (MDB_txn *, germ::block_hash const &);

    void account_put (MDB_txn *, germ::account const &, germ::account_info const &);
    bool account_get (MDB_txn *, germ::account const &, germ::account_info &);
    void account_del (MDB_txn *, germ::account const &);
    bool account_exists (MDB_txn *, germ::account const &);
    size_t account_count (MDB_txn *);
    germ::store_iterator latest_begin (MDB_txn *, germ::account const &);
    germ::store_iterator latest_begin (MDB_txn *);
    germ::store_iterator latest_end ();

    void pending_put (MDB_txn *, germ::pending_key const &, germ::pending_info const &);
    void pending_del (MDB_txn *, germ::pending_key const &);
    bool pending_get (MDB_txn *, germ::pending_key const &, germ::pending_info &);
    bool pending_exists (MDB_txn *, germ::pending_key const &);
    germ::store_iterator pending_begin (MDB_txn *, germ::pending_key const &);
    germ::store_iterator pending_begin (MDB_txn *);
    germ::store_iterator pending_end ();

    void block_info_put (MDB_txn *, germ::block_hash const &, germ::block_info const &);
    void block_info_del (MDB_txn *, germ::block_hash const &);
    bool block_info_get (MDB_txn *, germ::block_hash const &, germ::block_info &);
    bool block_info_exists (MDB_txn *, germ::block_hash const &);
    germ::store_iterator block_info_begin (MDB_txn *, germ::block_hash const &);
    germ::store_iterator block_info_begin (MDB_txn *);
    germ::store_iterator block_info_end ();
    germ::uint128_t block_balance (MDB_txn *, germ::block_hash const &);
    static size_t const block_info_max = 32;

//    germ::uint128_t representation_get (MDB_txn *, germ::account const &);
//    void representation_put (MDB_txn *, germ::account const &, germ::uint128_t const &);
//    void representation_add (MDB_txn *, germ::account const &, germ::uint128_t const &);
//    germ::store_iterator representation_begin (MDB_txn *);
//    germ::store_iterator representation_end ();

    void unchecked_clear (MDB_txn *);
    void unchecked_put (MDB_txn *, germ::block_hash const &, std::shared_ptr<germ::tx> const &);
    std::vector<std::shared_ptr<germ::tx>> unchecked_get (MDB_txn *, germ::block_hash const &);
    void unchecked_del (MDB_txn *, germ::block_hash const &, germ::tx const &);
    germ::store_iterator unchecked_begin (MDB_txn *);
    germ::store_iterator unchecked_begin (MDB_txn *, germ::block_hash const &);
    germ::store_iterator unchecked_end ();
    size_t unchecked_count (MDB_txn *);
    std::unordered_multimap<germ::block_hash, std::shared_ptr<germ::tx>> unchecked_cache;

    void checksum_put (MDB_txn *, uint64_t, uint8_t, germ::checksum const &);
    bool checksum_get (MDB_txn *, uint64_t, uint8_t, germ::checksum &);
    void checksum_del (MDB_txn *, uint64_t, uint8_t);

    // Return latest vote for an account from store
    std::shared_ptr<germ::vote> vote_get (MDB_txn *, germ::account const &);
    // Populate vote with the next sequence number
    std::shared_ptr<germ::vote> vote_generate (MDB_txn *, germ::account const &, germ::raw_key const &, std::shared_ptr<germ::tx>);
    // Return either vote or the stored vote with a higher sequence number
    std::shared_ptr<germ::vote> vote_max (MDB_txn *, std::shared_ptr<germ::vote>);
    // Return latest vote for an account considering the vote cache
    std::shared_ptr<germ::vote> vote_current (MDB_txn *, germ::account const &);
    void flush (MDB_txn *);
    germ::store_iterator vote_begin (MDB_txn *);
    germ::store_iterator vote_end ();
    std::mutex cache_mutex;
    std::unordered_map<germ::account, std::shared_ptr<germ::vote>> vote_cache;

    void version_put (MDB_txn *, int);
    int version_get (MDB_txn *);
    void do_upgrades (MDB_txn *);
    void upgrade_v1_to_v2 (MDB_txn *);
    void upgrade_v2_to_v3 (MDB_txn *);
    void upgrade_v3_to_v4 (MDB_txn *);
    void upgrade_v4_to_v5 (MDB_txn *);
    void upgrade_v5_to_v6 (MDB_txn *);
    void upgrade_v6_to_v7 (MDB_txn *);
    void upgrade_v7_to_v8 (MDB_txn *);
    void upgrade_v8_to_v9 (MDB_txn *);
    void upgrade_v9_to_v10 (MDB_txn *);
    void upgrade_v10_to_v11 (MDB_txn *);

    // Requires a write transaction
    germ::raw_key get_node_id (MDB_txn *);

    /** Deletes the node ID from the store */
    void delete_node_id (MDB_txn *);

    void clear (MDB_dbi);

    germ::mdb_env environment;

    /**
     * Maps head block to owning account
     * germ::block_hash -> germ::account
     */
    MDB_dbi frontiers;

    /**
     * Maps account to account information, head,             rep,              open,             balance,      timestamp and block count.
     * germ::account ->                     germ::block_hash, germ::block_hash, germ::block_hash, germ::amount, uint64_t,     uint64_t
     */
    MDB_dbi accounts;

    /**
     * Maps block hash to send block.
     * germ::block_hash -> germ::send_block
     */
    MDB_dbi send_blocks;

    /**
     * Maps block hash to receive block.
     * germ::block_hash -> germ::receive_block
     */
    MDB_dbi receive_blocks;

    /**
     * Maps block hash to open block.
     * germ::block_hash -> germ::open_block
     */
    MDB_dbi open_blocks;

    /**
     * Maps block hash to change block.
     * germ::block_hash -> germ::change_block
     */
    MDB_dbi change_blocks;

    /**
     * Maps block hash to state block.
     * germ::block_hash -> germ::state_block
     */
    MDB_dbi state_blocks;

    /**
     * Maps (destination account, pending block) to (source account, amount).
     * germ::account, germ::block_hash -> germ::account, germ::amount
     */
    MDB_dbi pending;

    /**
     * Maps block hash to account and balance.
     * block_hash -> germ::account, germ::amount
     */
    MDB_dbi blocks_info;

    /**
     * Representative weights.
     * germ::account -> germ::uint128_t
     */
    MDB_dbi representation;

    /**
     * Unchecked bootstrap blocks.
     * germ::block_hash -> germ::tx
     */
    MDB_dbi unchecked;

    /**
     * Mapping of region to checksum.
     * (uint56_t, uint8_t) -> germ::block_hash
     */
    MDB_dbi checksum;

    /**
     * Highest vote observed for account.
     * germ::account -> uint64_t
     */
    MDB_dbi vote;

    /**
     * Meta information about block store, such as versions.
     * germ::uint256_union (arbitrary key) -> blob
     */
    MDB_dbi meta;
};

}
