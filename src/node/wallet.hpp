#pragma once

#include <src/blockstore.hpp>
#include <src/common.hpp>
#include <src/node/common.hpp>
#include <src/node/openclwork.hpp>

#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

namespace germ
{
// The fan spreads a key out over the heap to decrease the likelihood of it being recovered by memory inspection
class fan
{
public:
    fan (germ::uint256_union const &, size_t);
    void value (germ::raw_key &);
    void value_set (germ::raw_key const &);
    std::vector<std::unique_ptr<germ::uint256_union>> values;

private:
    std::mutex mutex;
    void value_get (germ::raw_key &);
};
class wallet_value
{
public:
    wallet_value () = default;
    wallet_value (germ::mdb_val const &);
    wallet_value (germ::uint256_union const &, uint64_t);
    germ::mdb_val val () const;
    germ::private_key key;
    uint64_t work;
};
class node_config;
class kdf
{
public:
    void phs (germ::raw_key &, std::string const &, germ::uint256_union const &);
    std::mutex mutex;
};
enum class key_type
{
    not_a_type,
    unknown,
    adhoc,
    deterministic
};
class wallet_store
{
public:
    wallet_store (bool &, germ::kdf &, germ::transaction &, germ::account, unsigned, std::string const &);
    wallet_store (bool &, germ::kdf &, germ::transaction &, germ::account, unsigned, std::string const &, std::string const &);
    std::vector<germ::account> accounts (MDB_txn *);
    void initialize (MDB_txn *, bool &, std::string const &);
    germ::uint256_union check (MDB_txn *);
    bool rekey (MDB_txn *, std::string const &);
    bool valid_password (MDB_txn *);
    bool attempt_password (MDB_txn *, std::string const &);
    void wallet_key (germ::raw_key &, MDB_txn *);
    void seed (germ::raw_key &, MDB_txn *);
    void seed_set (MDB_txn *, germ::raw_key const &);
    germ::key_type key_type (germ::wallet_value const &);
    germ::public_key deterministic_insert (MDB_txn *);
    void deterministic_key (germ::raw_key &, MDB_txn *, uint32_t);
    uint32_t deterministic_index_get (MDB_txn *);
    void deterministic_index_set (MDB_txn *, uint32_t);
    void deterministic_clear (MDB_txn *);
    germ::uint256_union salt (MDB_txn *);
    bool is_representative (MDB_txn *);
    germ::account representative (MDB_txn *);
    void representative_set (MDB_txn *, germ::account const &);
    germ::public_key insert_adhoc (MDB_txn *, germ::raw_key const &);
    void insert_watch (MDB_txn *, germ::public_key const &);
    void erase (MDB_txn *, germ::public_key const &);
    germ::wallet_value entry_get_raw (MDB_txn *, germ::public_key const &);
    void entry_put_raw (MDB_txn *, germ::public_key const &, germ::wallet_value const &);
    bool fetch (MDB_txn *, germ::public_key const &, germ::raw_key &);
    bool exists (MDB_txn *, germ::public_key const &);
    void destroy (MDB_txn *);
    germ::store_iterator find (MDB_txn *, germ::uint256_union const &);
    germ::store_iterator begin (MDB_txn *, germ::uint256_union const &);
    germ::store_iterator begin (MDB_txn *);
    germ::store_iterator end ();
    void derive_key (germ::raw_key &, MDB_txn *, std::string const &);
    void serialize_json (MDB_txn *, std::string &);
    void write_backup (MDB_txn *, boost::filesystem::path const &);
    bool move (MDB_txn *, germ::wallet_store &, std::vector<germ::public_key> const &);
    bool import (MDB_txn *, germ::wallet_store &);
    bool work_get (MDB_txn *, germ::public_key const &, uint64_t &);
    void work_put (MDB_txn *, germ::public_key const &, uint64_t);
    unsigned version (MDB_txn *);
    void version_put (MDB_txn *, unsigned);
    void upgrade_v1_v2 ();
    void upgrade_v2_v3 ();
    germ::fan password;
    germ::fan wallet_key_mem;
    static unsigned const version_1;
    static unsigned const version_2;
    static unsigned const version_3;
    static unsigned const version_current;
    static germ::uint256_union const version_special;
    static germ::uint256_union const wallet_key_special;
    static germ::uint256_union const salt_special;
    static germ::uint256_union const check_special;
    static germ::uint256_union const representative_special;
    static germ::uint256_union const seed_special;
    static germ::uint256_union const deterministic_index_special;
    static int const special_count;
    static unsigned const kdf_full_work = 64 * 1024;
    static unsigned const kdf_test_work = 8;
    static unsigned const kdf_work = germ::rai_network == germ::germ_networks::germ_test_network ? kdf_test_work : kdf_full_work;
    germ::kdf & kdf;
    germ::mdb_env & environment;
    MDB_dbi handle;
    std::recursive_mutex mutex;
};
class node;
// A wallet is a set of account keys encrypted by a common encryption key
class wallet : public std::enable_shared_from_this<germ::wallet>
{
public:
    std::shared_ptr<germ::tx> change_action (germ::account const &, germ::account const &, bool = true);
    std::shared_ptr<germ::tx> receive_action (germ::tx const &, germ::account const &, germ::uint128_union const &, bool = true);
    std::shared_ptr<germ::tx> send_action (germ::account const &, germ::account const &, germ::uint128_t const &, bool = true, boost::optional<std::string> = {});
    wallet (bool &, germ::transaction &, germ::node &, std::string const &);
    wallet (bool &, germ::transaction &, germ::node &, std::string const &, std::string const &);
    void enter_initial_password ();
    bool valid_password ();
    bool enter_password (std::string const &);
    germ::public_key insert_adhoc (germ::raw_key const &, bool = true);
    germ::public_key insert_adhoc (MDB_txn *, germ::raw_key const &, bool = true);
    void insert_watch (MDB_txn *, germ::public_key const &);
    germ::public_key deterministic_insert (MDB_txn *, bool = true);
    germ::public_key deterministic_insert (bool = true);
    bool exists (germ::public_key const &);
    bool import (std::string const &, std::string const &);
    void serialize (std::string &);
    bool change_sync (germ::account const &, germ::account const &);
    void change_async (germ::account const &, germ::account const &, std::function<void(std::shared_ptr<germ::tx>)> const &, bool = true);
    bool receive_sync (std::shared_ptr<germ::tx>, germ::account const &, germ::uint128_t const &);
    void receive_async (std::shared_ptr<germ::tx>, germ::account const &, germ::uint128_t const &, std::function<void(std::shared_ptr<germ::tx>)> const &, bool = true);
    germ::block_hash send_sync (germ::account const &, germ::account const &, germ::uint128_t const &);
    void send_async (germ::account const &, germ::account const &, germ::uint128_t const &, std::function<void(std::shared_ptr<germ::tx>)> const &, bool = true, boost::optional<std::string> = {});
    void work_apply (germ::account const &, std::function<void(uint64_t)>);
    void work_cache_blocking (germ::account const &, germ::block_hash const &);
    void work_update (MDB_txn *, germ::account const &, germ::block_hash const &, uint64_t);
    void work_ensure (germ::account const &, germ::block_hash const &);
    bool search_pending ();
    void init_free_accounts (MDB_txn *);
    /** Changes the wallet seed and returns the first account */
    germ::public_key change_seed (MDB_txn * transaction_a, germ::raw_key const & prv_a);
    std::unordered_set<germ::account> free_accounts;
    std::function<void(bool, bool)> lock_observer;
    germ::wallet_store store;
    germ::node & node;
};
// The wallets set is all the wallets a node controls.  A node may contain multiple wallets independently encrypted and operated.
class wallets
{
public:
    wallets (bool &, germ::node &);
    ~wallets ();
    std::shared_ptr<germ::wallet> open (germ::uint256_union const &);
    std::shared_ptr<germ::wallet> create (germ::uint256_union const &);
    bool search_pending (germ::uint256_union const &);
    void search_pending_all ();
    void destroy (germ::uint256_union const &);
    void do_wallet_actions ();
    void queue_wallet_action (germ::uint128_t const &, std::function<void()> const &);
    void foreach_representative (MDB_txn *, std::function<void(germ::public_key const &, germ::raw_key const &)> const &);
    bool exists (MDB_txn *, germ::public_key const &);
    void stop ();
    std::function<void(bool)> observer;
    std::unordered_map<germ::uint256_union, std::shared_ptr<germ::wallet>> items;
    std::multimap<germ::uint128_t, std::function<void()>, std::greater<germ::uint128_t>> actions;
    std::mutex mutex;
    std::condition_variable condition;
    germ::kdf kdf;
    MDB_dbi handle;
    MDB_dbi send_action_ids;
    germ::node & node;
    bool stopped;
    std::thread thread;
    static germ::uint128_t const generate_priority;
    static germ::uint128_t const high_priority;
};
}
