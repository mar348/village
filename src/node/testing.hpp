#pragma once

#include <src/node/node.hpp>

namespace germ
{
class system
{
public:
    system (uint16_t, size_t);
    ~system ();
    void generate_activity (germ::node &, std::vector<germ::account> &);
    void generate_mass_activity (uint32_t, germ::node &);
    void generate_usage_traffic (uint32_t, uint32_t, size_t);
    void generate_usage_traffic (uint32_t, uint32_t);
    germ::account get_random_account (std::vector<germ::account> &);
    germ::uint128_t get_random_amount (MDB_txn *, germ::node &, germ::account const &);
    void generate_rollback (germ::node &, std::vector<germ::account> &);
    void generate_change_known (germ::node &, std::vector<germ::account> &);
    void generate_change_unknown (germ::node &, std::vector<germ::account> &);
    void generate_receive (germ::node &);
    void generate_send_new (germ::node &, std::vector<germ::account> &);
    void generate_send_existing (germ::node &, std::vector<germ::account> &);
    std::shared_ptr<germ::wallet> wallet (size_t);
    germ::account account (MDB_txn *, size_t);
    void poll ();
    void stop ();
    boost::asio::io_service service;
    germ::alarm alarm;
    std::vector<std::shared_ptr<germ::node>> nodes;
    germ::logging logging;
    germ::work_pool work;
};
class landing_store
{
public:
    landing_store ();
    landing_store (germ::account const &, germ::account const &, uint64_t, uint64_t);
    landing_store (bool &, std::istream &);
    germ::account source;
    germ::account destination;
    uint64_t start;
    uint64_t last;
    bool deserialize (std::istream &);
    void serialize (std::ostream &) const;
    bool operator== (germ::landing_store const &) const;
};
class landing
{
public:
    landing (germ::node &, std::shared_ptr<germ::wallet>, germ::landing_store &, boost::filesystem::path const &);
    void write_store ();
    germ::uint128_t distribution_amount (uint64_t);
    void distribute_one ();
    void distribute_ongoing ();
    boost::filesystem::path path;
    germ::landing_store & store;
    std::shared_ptr<germ::wallet> wallet;
    germ::node & node;
    static int constexpr interval_exponent = 10;
    static std::chrono::seconds constexpr distribution_interval = std::chrono::seconds (1 << interval_exponent); // 1024 seconds
    static std::chrono::seconds constexpr sleep_seconds = std::chrono::seconds (7);
};
}
