#pragma once


#include <boost/log/sources/logger.hpp>

#include <src/ledger.hpp>
#include <src/lib/work.hpp>
#include <src/node/stats.hpp>
#include <src/node/wallet.hpp>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_set>

#include <boost/asio.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/log/trivial.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index_container.hpp>

#include <miniupnpc.h>

#include <src/epochstore.h>
#include <src/node/active_elections.h>
#include <src/node/bootstrap/bootstrap_listener.h>
#include <src/node/bootstrap/bootstrap_initiator.h>


namespace boost
{
namespace program_options
{
    class options_description;
    class variables_map;
}
}

namespace germ
{

germ::endpoint map_endpoint_to_v6 (germ::endpoint const &);
class node;
class election_status
{
public:
    std::shared_ptr<germ::tx> winner;
    germ::amount tally;
};
class vote_info
{
public:
    std::chrono::steady_clock::time_point time;
    uint64_t sequence;
    germ::block_hash hash;
    bool operator< (germ::vote const &) const;
};
class election : public std::enable_shared_from_this<germ::election>
{
    std::function<void(std::shared_ptr<germ::tx>)> confirmation_action;
    void confirm_once (MDB_txn *);

public:
    election (germ::node &, std::shared_ptr<germ::tx>, std::function<void(std::shared_ptr<germ::tx>)> const &);
    bool vote (std::shared_ptr<germ::vote>);
    // Check if we have vote quorum
    bool have_quorum (germ::tally_t const &);
    // Tell the network our view of the winner
    void broadcast_winner ();
    // Change our winner to agree with the network
    void compute_rep_votes (MDB_txn *);
    // Confirm this block if quorum is met
    void confirm_if_quorum (MDB_txn *);
    germ::votes votes;
    germ::node & node;
    std::unordered_map<germ::account, germ::vote_info> last_votes;
    germ::election_status status;
    std::atomic<bool> confirmed;
};
class conflict_info
{
public:
    germ::block_hash root;
    std::shared_ptr<germ::election> election;
    // Number of announcements in a row for this fork
    unsigned announcements;
    std::pair<std::shared_ptr<germ::tx>, std::shared_ptr<germ::tx>> confirm_req_options;
};
// Core class for determining consensus
// Holds all active blocks i.e. recently added blocks that need confirmation
class active_transactions
{
public:
    active_transactions (germ::node &);
    // Start an election for a block
    // Call action with confirmed block, may be different than what we started with
    bool start (std::shared_ptr<germ::tx>, std::function<void(std::shared_ptr<germ::tx>)> const & = [](std::shared_ptr<germ::tx>) {});
    // Also supply alternatives to block, to confirm_req reps with if the boolean argument is true
    // Should only be used for old elections
    // The first block should be the one in the ledger
    bool start (std::pair<std::shared_ptr<germ::tx>, std::shared_ptr<germ::tx>>, std::function<void(std::shared_ptr<germ::tx>)> const & = [](std::shared_ptr<germ::tx>) {});
    // If this returns true, the vote is a replay
    // If this returns false, the vote may or may not be a replay
    bool vote (std::shared_ptr<germ::vote>);
    // Is the root of this block in the roots container
    bool active (germ::tx const &);
    void announce_votes ();
    std::deque<std::shared_ptr<germ::tx>> list_blocks ();
    void erase (germ::tx const &);
    void stop ();
    boost::multi_index_container<
    germ::conflict_info,
    boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<boost::multi_index::member<germ::conflict_info, germ::block_hash, &germ::conflict_info::root>>>>
    roots;
    std::deque<germ::election_status> confirmed;
    germ::node & node;
    std::mutex mutex;
    // Maximum number of conflicts to vote on per interval, lowest root hash first
    static unsigned constexpr announcements_per_interval = 32;
    // Minimum number of block announcements
    static unsigned constexpr announcement_min = 4;
    // Threshold to start logging blocks haven't yet been confirmed
    static unsigned constexpr announcement_long = 20;
    static unsigned constexpr announce_interval_ms = (germ::rai_network == germ::germ_networks::germ_test_network) ? 10 : 16000;
    static size_t constexpr election_history_size = 2048;
};
class operation
{
public:
    bool operator> (germ::operation const &) const;
    std::chrono::steady_clock::time_point wakeup;
    std::function<void()> function;
};
class alarm
{
public:
    alarm (boost::asio::io_service &);
    ~alarm ();
    void add (std::chrono::steady_clock::time_point const &, std::function<void()> const &);
    void run ();
    boost::asio::io_service & service;
    std::mutex mutex;
    std::condition_variable condition;
    std::priority_queue<operation, std::vector<operation>, std::greater<operation>> operations;
    std::thread thread;
};
class gap_information
{
public:
    std::chrono::steady_clock::time_point arrival;
    germ::block_hash hash;
    std::unique_ptr<germ::votes> votes;
};
class gap_cache
{
public:
    gap_cache (germ::node &);
    void add (MDB_txn *, std::shared_ptr<germ::tx>);
    void vote (std::shared_ptr<germ::vote>);
    germ::uint128_t bootstrap_threshold (MDB_txn *);
    void purge_old ();
    boost::multi_index_container<
    germ::gap_information,
    boost::multi_index::indexed_by<
    boost::multi_index::ordered_non_unique<boost::multi_index::member<gap_information, std::chrono::steady_clock::time_point, &gap_information::arrival>>,
    boost::multi_index::hashed_unique<boost::multi_index::member<gap_information, germ::block_hash, &gap_information::hash>>>>
    blocks;
    size_t const max = 256;
    std::mutex mutex;
    germ::node & node;
};
class work_pool;
class peer_information
{
public:
    peer_information (germ::endpoint const &, unsigned);
    peer_information (germ::endpoint const &, std::chrono::steady_clock::time_point const &, std::chrono::steady_clock::time_point const &);
    germ::endpoint endpoint;
    boost::asio::ip::address ip_address;
    std::chrono::steady_clock::time_point last_contact;
    std::chrono::steady_clock::time_point last_attempt;
    std::chrono::steady_clock::time_point last_bootstrap_attempt;
    std::chrono::steady_clock::time_point last_rep_request;
    std::chrono::steady_clock::time_point last_rep_response;
    germ::amount rep_weight;
    germ::account probable_rep_account;
    unsigned network_version;
    boost::optional<germ::account> node_id;
};
class peer_attempt
{
public:
    germ::endpoint endpoint;
    std::chrono::steady_clock::time_point last_attempt;
};
class syn_cookie_info
{
public:
    germ::uint256_union cookie;
    std::chrono::steady_clock::time_point created_at;
};
class peer_by_ip_addr
{
};
class peer_container
{
public:
    peer_container (germ::endpoint const &);
    // We were contacted by endpoint, update peers
    // Returns true if a Node ID handshake should begin
    bool contacted (germ::endpoint const &, unsigned);
    // Unassigned, reserved, self
    bool not_a_peer (germ::endpoint const &, bool);
    // Returns true if peer was already known
    bool known_peer (germ::endpoint const &);
    // Notify of peer we received from
    bool insert (germ::endpoint const &, unsigned);
    std::unordered_set<germ::endpoint> random_set (size_t);
    void random_fill (std::array<germ::endpoint, 8> &);
    // Request a list of the top known representatives
    std::vector<peer_information> representatives (size_t);
    // List of all peers
    std::deque<germ::endpoint> list ();
    std::map<germ::endpoint, unsigned> list_version ();
    std::vector<peer_information> list_vector ();
    // A list of random peers sized for the configured rebroadcast fanout
    std::deque<germ::endpoint> list_fanout ();
    // Get the next peer for attempting bootstrap
    germ::endpoint bootstrap_peer ();
    // Purge any peer where last_contact < time_point and return what was left
    std::vector<germ::peer_information> purge_list (std::chrono::steady_clock::time_point const &);
    void purge_syn_cookies (std::chrono::steady_clock::time_point const &);
    std::vector<germ::endpoint> rep_crawl ();
    bool rep_response (germ::endpoint const &, germ::account const &, germ::amount const &);
    void rep_request (germ::endpoint const &);
    // Should we reach out to this endpoint with a keepalive message
    bool reachout (germ::endpoint const &);
    // Returns boost::none if the IP is rate capped on syn cookie requests,
    // or if the endpoint already has a syn cookie query
    boost::optional<germ::uint256_union> assign_syn_cookie (germ::endpoint const &);
    // Returns false if valid, true if invalid (true on error convention)
    // Also removes the syn cookie from the store if valid
    bool validate_syn_cookie (germ::endpoint const &, germ::account, germ::signature);
    size_t size ();
    size_t size_sqrt ();
    bool empty ();
    std::mutex mutex;
    germ::endpoint self;
    boost::multi_index_container<
    peer_information,
    boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<boost::multi_index::member<peer_information, germ::endpoint, &peer_information::endpoint>>,
    boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_contact>>,
    boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_attempt>, std::greater<std::chrono::steady_clock::time_point>>,
    boost::multi_index::random_access<>,
    boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_bootstrap_attempt>>,
    boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, std::chrono::steady_clock::time_point, &peer_information::last_rep_request>>,
    boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_information, germ::amount, &peer_information::rep_weight>, std::greater<germ::amount>>,
    boost::multi_index::ordered_non_unique<boost::multi_index::tag<peer_by_ip_addr>, boost::multi_index::member<peer_information, boost::asio::ip::address, &peer_information::ip_address>>>>
    peers;
    boost::multi_index_container<
    peer_attempt,
    boost::multi_index::indexed_by<
    boost::multi_index::hashed_unique<boost::multi_index::member<peer_attempt, germ::endpoint, &peer_attempt::endpoint>>,
    boost::multi_index::ordered_non_unique<boost::multi_index::member<peer_attempt, std::chrono::steady_clock::time_point, &peer_attempt::last_attempt>>>>
    attempts;
    std::mutex syn_cookie_mutex;
    std::unordered_map<germ::endpoint, syn_cookie_info> syn_cookies;
    std::unordered_map<boost::asio::ip::address, unsigned> syn_cookies_per_ip;
    // Number of peers that don't support node ID
    size_t legacy_peers;
    // Called when a new peer is observed
    std::function<void(germ::endpoint const &)> peer_observer;
    std::function<void()> disconnect_observer;
    // Number of peers to crawl for being a rep every period
    static size_t constexpr peers_per_crawl = 8;
    // Maximum number of peers per IP (includes legacy peers)
    static size_t constexpr max_peers_per_ip = 10;
    // Maximum number of legacy peers per IP
    static size_t constexpr max_legacy_peers_per_ip = 5;
    // Maximum number of peers that don't support node ID
    static size_t constexpr max_legacy_peers = 500;
};
class send_info
{
public:
    uint8_t const * data;
    size_t size;
    germ::endpoint endpoint;
    std::function<void(boost::system::error_code const &, size_t)> callback;
};
class mapping_protocol
{
public:
    char const * name;
    int remaining;
    boost::asio::ip::address_v4 external_address;
    uint16_t external_port;
};
// These APIs aren't easy to understand so comments are verbose
class port_mapping
{
public:
    port_mapping (germ::node &);
    void start ();
    void stop ();
    void refresh_devices ();
    // Refresh when the lease ends
    void refresh_mapping ();
    // Refresh occasionally in case router loses mapping
    void check_mapping_loop ();
    int check_mapping ();
    bool has_address ();
    std::mutex mutex;
    germ::node & node;
    UPNPDev * devices; // List of all UPnP devices
    UPNPUrls urls; // Something for UPnP
    IGDdatas data; // Some other UPnP thing
    // Primes so they infrequently happen at the same time
    static int constexpr mapping_timeout = germ::rai_network == germ::germ_networks::germ_test_network ? 53 : 3593;
    static int constexpr check_timeout = germ::rai_network == germ::germ_networks::germ_test_network ? 17 : 53;
    boost::asio::ip::address_v4 address;
    std::array<mapping_protocol, 2> protocols;
    uint64_t check_count;
    bool on;
};
class block_arrival_info
{
public:
    std::chrono::steady_clock::time_point arrival;
    germ::block_hash hash;
};
// This class tracks blocks that are probably live because they arrived in a UDP packet
// This gives a fairly reliable way to differentiate between blocks being inserted via bootstrap or new, live blocks.
class block_arrival
{
public:
    // Return `true' to indicated an error if the block has already been inserted
    bool add (germ::block_hash const &);
    bool recent (germ::block_hash const &);
    boost::multi_index_container<
    germ::block_arrival_info,
    boost::multi_index::indexed_by<
    boost::multi_index::ordered_non_unique<boost::multi_index::member<germ::block_arrival_info, std::chrono::steady_clock::time_point, &germ::block_arrival_info::arrival>>,
    boost::multi_index::hashed_unique<boost::multi_index::member<germ::block_arrival_info, germ::block_hash, &germ::block_arrival_info::hash>>>>
    arrival;
    std::mutex mutex;
    static size_t constexpr arrival_size_min = 8 * 1024;
    static std::chrono::seconds constexpr arrival_time_min = std::chrono::seconds (300);
};
class rep_last_heard_info
{
public:
    std::chrono::steady_clock::time_point last_heard;
    germ::account representative;
};
class online_reps
{
public:
    online_reps (germ::node &);
    void vote (std::shared_ptr<germ::vote> const &);
    void recalculate_stake ();
    germ::uint128_t online_stake ();
    std::deque<germ::account> list ();
    boost::multi_index_container<
    germ::rep_last_heard_info,
    boost::multi_index::indexed_by<
    boost::multi_index::ordered_non_unique<boost::multi_index::member<germ::rep_last_heard_info, std::chrono::steady_clock::time_point, &germ::rep_last_heard_info::last_heard>>,
    boost::multi_index::hashed_unique<boost::multi_index::member<germ::rep_last_heard_info, germ::account, &germ::rep_last_heard_info::representative>>>>
    reps;

private:
    germ::uint128_t online_stake_total;
    std::mutex mutex;
    germ::node & node;
};
class network
{
public:
    network (germ::node &, uint16_t);
    void receive ();
    void stop ();
    void receive_action (boost::system::error_code const &, size_t);
    void rpc_action (boost::system::error_code const &, size_t);
    void republish_vote (std::shared_ptr<germ::vote>);
    void republish_block (MDB_txn *, std::shared_ptr<germ::tx>);
    void republish (germ::block_hash const &, std::shared_ptr<std::vector<uint8_t>>, germ::endpoint);
    void publish_broadcast (std::vector<germ::peer_information> &, std::unique_ptr<germ::tx>);
    void confirm_send (germ::confirm_ack const &, std::shared_ptr<std::vector<uint8_t>>, germ::endpoint const &);
    void merge_peers (std::array<germ::endpoint, 8> const &);
    void send_keepalive (germ::endpoint const &);
    void send_node_id_handshake (germ::endpoint const &, boost::optional<germ::uint256_union> const & query, boost::optional<germ::uint256_union> const & respond_to);
    void broadcast_confirm_req (std::shared_ptr<germ::tx>);
    void broadcast_confirm_req_base (std::shared_ptr<germ::tx>, std::shared_ptr<std::vector<germ::peer_information>>, unsigned);
    void send_confirm_req (germ::endpoint const &, std::shared_ptr<germ::tx>);
    void send_buffer (uint8_t const *, size_t, germ::endpoint const &, std::function<void(boost::system::error_code const &, size_t)>);
    germ::endpoint endpoint ();
    germ::endpoint remote;
    std::array<uint8_t, 512> buffer;
    boost::asio::ip::udp::socket socket;
    std::mutex socket_mutex;
    boost::asio::ip::udp::resolver resolver;
    germ::node & node;
    bool on;
    static uint16_t const node_port = germ::rai_network == germ::germ_networks::germ_live_network ? 7075 : 54000;
};
class logging
{
public:
    logging ();
    void serialize_json (boost::property_tree::ptree &) const;
    bool deserialize_json (bool &, boost::property_tree::ptree &);
    bool upgrade_json (unsigned, boost::property_tree::ptree &);
    bool ledger_logging () const;
    bool ledger_duplicate_logging () const;
    bool vote_logging () const;
    bool network_logging () const;
    bool network_message_logging () const;
    bool network_publish_logging () const;
    bool network_packet_logging () const;
    bool network_keepalive_logging () const;
    bool network_node_id_handshake_logging () const;
    bool node_lifetime_tracing () const;
    bool insufficient_work_logging () const;
    bool log_rpc () const;
    bool bulk_pull_logging () const;
    bool epoch_bulk_pull_logging () const;
    bool callback_logging () const;
    bool work_generation_time () const;
    bool log_to_cerr () const;
    void init (boost::filesystem::path const &);

    bool ledger_logging_value;
    bool ledger_duplicate_logging_value;
    bool vote_logging_value;
    bool network_logging_value;
    bool network_message_logging_value;
    bool network_publish_logging_value;
    bool network_packet_logging_value;
    bool network_keepalive_logging_value;
    bool network_node_id_handshake_logging_value;
    bool node_lifetime_tracing_value;
    bool insufficient_work_logging_value;
    bool log_rpc_value;
    bool bulk_pull_logging_value;
    bool epoch_bulk_pull_logging_value;
    bool work_generation_time_value;
    bool log_to_cerr_value;
    bool flush;
    uintmax_t max_size;
    uintmax_t rotation_size;
    boost::log::sources::logger_mt log;
};
class node_init
{
public:
    node_init ();
    bool error ();
    bool block_store_init;
    bool wallet_init;
    bool epoch_store_init;
};
class node_config
{
public:
    node_config ();
    node_config (uint16_t, germ::logging const &);
    void serialize_json (boost::property_tree::ptree &) const;
    bool deserialize_json (bool &, boost::property_tree::ptree &);
    bool upgrade_json (unsigned, boost::property_tree::ptree &);
    germ::account random_representative ();
    uint16_t peering_port;
    germ::logging logging;
    std::vector<std::pair<std::string, uint16_t>> work_peers;
    std::vector<std::string> preconfigured_peers;
    std::vector<germ::account> preconfigured_representatives;
    unsigned bootstrap_fraction_numerator;
    germ::amount receive_minimum;
    germ::amount online_weight_minimum;
    unsigned online_weight_quorum;
    unsigned password_fanout;
    unsigned io_threads;
    unsigned work_threads;
    bool enable_voting;
    unsigned bootstrap_connections;
    unsigned bootstrap_connections_max;
    std::string callback_address;
    uint16_t callback_port;
    std::string callback_target;
    int lmdb_max_dbs;
    germ::stat_config stat_config;
    germ::block_hash state_block_parse_canary;
    germ::block_hash state_block_generate_canary;
    static std::chrono::seconds constexpr keepalive_period = std::chrono::seconds (60);
    static std::chrono::seconds constexpr keepalive_cutoff = keepalive_period * 5;
    static std::chrono::minutes constexpr wallet_backup_interval = std::chrono::minutes (5);
};
class node_observers
{
public:
    germ::observer_set<std::shared_ptr<germ::tx>, germ::account const &, germ::uint128_t const &, bool> blocks;
    germ::observer_set<bool> wallet;
    germ::observer_set<std::shared_ptr<germ::vote>, germ::endpoint const &> vote;
    germ::observer_set<germ::account const &, bool> account_balance;
    germ::observer_set<germ::endpoint const &> endpoint;
    germ::observer_set<> disconnect;
    germ::observer_set<> started;
};
class vote_processor
{
public:
    vote_processor (germ::node &);
    germ::vote_code vote (std::shared_ptr<germ::vote>, germ::endpoint);
    germ::node & node;
};
// The network is crawled for representatives by occasionally sending a unicast confirm_req for a specific block and watching to see if it's acknowledged with a vote.
class rep_crawler
{
public:
    void add (germ::block_hash const &);
    void remove (germ::block_hash const &);
    bool exists (germ::block_hash const &);
    std::mutex mutex;
    std::unordered_set<germ::block_hash> active;
};
// Processing blocks is a potentially long IO operation
// This class isolates block insertion from other operations like servicing network operations
class block_processor
{
public:
    block_processor (germ::node &);
    ~block_processor ();
    void stop ();
    void flush ();
    bool full ();
    void add (std::shared_ptr<germ::tx>, std::chrono::steady_clock::time_point);
    void force (std::shared_ptr<germ::tx>);
    bool should_log ();
    bool have_blocks ();
    void process_blocks ();
    germ::process_return process_receive_one (MDB_txn *, std::shared_ptr<germ::tx>, std::chrono::steady_clock::time_point = std::chrono::steady_clock::now ());

private:
    void queue_unchecked (MDB_txn *, germ::block_hash const &);
    void process_receive_many (std::unique_lock<std::mutex> &);
    bool stopped;
    bool active;
    std::chrono::steady_clock::time_point next_log;
    std::deque<std::pair<std::shared_ptr<germ::tx>, std::chrono::steady_clock::time_point>> blocks;
    std::deque<std::shared_ptr<germ::tx>> forced;
    std::condition_variable condition;
    germ::node & node;
    std::mutex mutex;
};
class node : public std::enable_shared_from_this<germ::node>
{
public:
    node (germ::node_init &, boost::asio::io_service &, uint16_t, boost::filesystem::path const &, germ::alarm &, germ::logging const &, germ::work_pool &);
    node (germ::node_init &, boost::asio::io_service &, boost::filesystem::path const &, germ::alarm &, germ::node_config const &, germ::work_pool &);
    ~node ();
    template <typename T>
    void background (T action_a)
    {
        alarm.service.post (action_a);
    }
    void send_keepalive (germ::endpoint const &);
    bool copy_with_compaction (boost::filesystem::path const &);
    void keepalive (std::string const &, uint16_t);
    void start ();
    void stop ();
    std::shared_ptr<germ::node> shared ();
    int store_version ();
    void process_confirmed (std::shared_ptr<germ::tx>);
    void process_message (germ::message &, germ::endpoint const &);
    void process_active (std::shared_ptr<germ::tx>);
    germ::process_return process (germ::tx const &);
    void keepalive_preconfigured (std::vector<std::string> const &);
    germ::block_hash latest (germ::account const &);
    germ::uint128_t balance (germ::account const &);
    std::unique_ptr<germ::tx> block (germ::block_hash const &);
    std::pair<germ::uint128_t, germ::uint128_t> balance_pending (germ::account const &);
    germ::uint128_t weight (germ::account const &);
    germ::account representative (germ::account const &);
    void ongoing_keepalive ();
    void ongoing_syn_cookie_cleanup ();
    void ongoing_rep_crawl ();
    void ongoing_bootstrap ();
    void ongoing_store_flush ();
    void backup_wallet ();
    int price (germ::uint128_t const &, int);
    void work_generate_blocking (germ::tx &);
    uint64_t work_generate_blocking (germ::uint256_union const &);
    void work_generate (germ::uint256_union const &, std::function<void(uint64_t)>);
    void add_initial_peers ();
    void block_confirm (std::shared_ptr<germ::tx>);
    void process_fork (MDB_txn *, std::shared_ptr<germ::tx>);
    germ::uint128_t delta ();
    boost::asio::io_service & service;
    germ::node_config config;
    germ::alarm & alarm;
    germ::work_pool & work;
    boost::log::sources::logger_mt log;
    germ::block_store store;
    germ::epoch_store epoch_store;
    germ::gap_cache gap_cache;
    germ::ledger ledger;
    germ::active_transactions active;
    germ::active_elections active_election;
    germ::network network;
    germ::tcp_bootstrap_initiator bootstrap_initiator;
    germ::tcp_bootstrap_listener bootstrap;
    germ::peer_container peers;
    boost::filesystem::path application_path;
    germ::node_observers observers;
    germ::wallets wallets;
    germ::port_mapping port_mapping;
    germ::vote_processor vote_processor;
    germ::rep_crawler rep_crawler;
    unsigned warmed_up;
    germ::block_processor block_processor;
    std::thread block_processor_thread;
    germ::block_arrival block_arrival;
    germ::online_reps online_reps;
    germ::stat stats;
    germ::keypair node_id;
    static double constexpr price_max = 16.0;
    static double constexpr free_cutoff = 1024.0;
    static std::chrono::seconds constexpr period = std::chrono::seconds (60);
    static std::chrono::seconds constexpr cutoff = period * 5;
    static std::chrono::seconds constexpr syn_cookie_cutoff = std::chrono::seconds (5);
    static std::chrono::minutes constexpr backup_interval = std::chrono::minutes (5);
};
class thread_runner
{
public:
    thread_runner (boost::asio::io_service &, unsigned);
    ~thread_runner ();
    void join ();
    std::vector<std::thread> threads;
};
void add_node_options (boost::program_options::options_description &);
bool handle_node_options (boost::program_options::variables_map &);
class inactive_node
{
public:
    inactive_node (boost::filesystem::path const & path = germ::working_path ());
    ~inactive_node ();
    boost::filesystem::path path;
    boost::shared_ptr<boost::asio::io_service> service;
    germ::alarm alarm;
    germ::logging logging;
    germ::node_init init;
    germ::work_pool work;
    std::shared_ptr<germ::node> node;
};
}
