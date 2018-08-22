//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BOOTSTRAP_ATTEMPT_H
#define SRC_BOOTSTRAP_ATTEMPT_H

#include <src/node/common.hpp>
#include <src/node/bootstrap/bootstrap.h>


namespace germ
{
class node;
class tcp_bootstrap_client;  
class tcp_frontier_req_client;
class tcp_bulk_push_client;
class tcp_bootstrap_attempt : public std::enable_shared_from_this<tcp_bootstrap_attempt>
{
public:
    tcp_bootstrap_attempt (std::shared_ptr<germ::node> node_a);
    ~tcp_bootstrap_attempt ();
    void run ();
    std::shared_ptr<germ::tcp_bootstrap_client> connection (std::unique_lock<std::mutex> &);
    bool consume_future (std::future<bool> &);
    void populate_connections ();
    bool request_frontier (std::unique_lock<std::mutex> &);
    void request_pull (std::unique_lock<std::mutex> &);
    void request_push (std::unique_lock<std::mutex> &);
    void add_connection (germ::endpoint const &);
    void pool_connection (std::shared_ptr<germ::tcp_bootstrap_client>);
    void stop ();
    void requeue_pull (germ::pull_info const &);
    void add_pull (germ::pull_info const &);
    bool still_pulling ();
    unsigned target_connections (size_t pulls_remaining);
    bool should_log ();
    void add_bulk_push_target (germ::block_hash const &, germ::block_hash const &);
    std::chrono::steady_clock::time_point next_log;
    std::deque<std::weak_ptr<germ::tcp_bootstrap_client>> clients;
    std::weak_ptr<germ::tcp_bootstrap_client> connection_frontier_request;
    std::weak_ptr<germ::tcp_frontier_req_client> frontiers;
    std::weak_ptr<germ::tcp_bulk_push_client> push;
    std::deque<germ::pull_info> pulls;
    std::deque<std::shared_ptr<germ::tcp_bootstrap_client>> idle;
    std::atomic<unsigned> connections;
    std::atomic<unsigned> pulling;
    std::shared_ptr<germ::node> node;
    std::atomic<unsigned> account_count;
    std::atomic<uint64_t> total_blocks;
    std::vector<std::pair<germ::block_hash, germ::block_hash>> bulk_push_targets;
    bool stopped;
    std::mutex mutex;
    std::condition_variable condition;
};


}


#endif //SRC_BOOTSTRAP_ATTEMPT_H
