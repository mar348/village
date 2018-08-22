#pragma once

#include <src/common.hpp>

namespace germ
{
class block_store;
class stat;

class shared_ptr_block_hash
{
public:
    size_t operator() (std::shared_ptr<germ::tx> const &) const;
    bool operator() (std::shared_ptr<germ::tx> const &, std::shared_ptr<germ::tx> const &) const;
};
using tally_t = std::map<germ::uint128_t, std::shared_ptr<germ::tx>, std::greater<germ::uint128_t>>;
class ledger
{
public:
    ledger (germ::block_store &, germ::stat &);
    std::pair<germ::uint128_t, std::shared_ptr<germ::tx>> winner (MDB_txn *, germ::votes const & votes_a);
    // Map of weight -> associated block, ordered greatest to least
    germ::tally_t tally (MDB_txn *, germ::votes const &);
    germ::account account (MDB_txn *, germ::block_hash const &);
    germ::uint128_t amount (MDB_txn *, germ::block_hash const &);
    germ::uint128_t balance (MDB_txn *, germ::block_hash const &);
    germ::uint128_t account_balance (MDB_txn *, germ::account const &);
    germ::uint128_t account_pending (MDB_txn *, germ::account const &);
    germ::uint128_t weight (MDB_txn *, germ::account const &);
    std::unique_ptr<germ::tx> successor (MDB_txn *, germ::block_hash const &);
    std::unique_ptr<germ::tx> forked_block (MDB_txn *, germ::tx const &);
    germ::block_hash latest (MDB_txn *, germ::account const &);
    germ::block_hash latest_root (MDB_txn *, germ::account const &);
//    germ::block_hash representative (MDB_txn *, germ::block_hash const &);
//    germ::block_hash representative_calculated (MDB_txn *, germ::block_hash const &);
    bool block_exists (germ::block_hash const &);
    std::string block_text (char const *);
    std::string block_text (germ::block_hash const &);
    bool is_send (MDB_txn *, germ::tx const &);
    germ::block_hash block_destination (MDB_txn *, germ::tx const &);
    germ::block_hash block_source (MDB_txn *, germ::tx const &);
    germ::process_return process (MDB_txn *, germ::tx const &);
    void rollback (MDB_txn *, germ::block_hash const &);
    void change_latest (MDB_txn *, germ::account const &, germ::block_hash const &, /*germ::account const &,*/ germ::uint128_union const &, uint64_t, bool = false);
    void checksum_update (MDB_txn *, germ::block_hash const &);
    germ::checksum checksum (MDB_txn *, germ::account const &, germ::account const &);
    void dump_account_chain (germ::account const &);
    static germ::uint128_t const unit;
    germ::block_store & store;
    germ::stat & stats;
    std::unordered_map<germ::account, germ::uint128_t> bootstrap_weights;
    uint64_t bootstrap_weight_max_blocks;
    std::atomic<bool> check_bootstrap_weights;
};
};
