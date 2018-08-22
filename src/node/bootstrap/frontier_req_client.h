//
// Created by daoful on 18-7-26.
//

#ifndef SRC_FRONTIER_REQ_CLIENT_H
#define SRC_FRONTIER_REQ_CLIENT_H


#include <src/node/bootstrap/bootstrap.h>


namespace germ
{
class node;
class tcp_bootstrap_client;
class tcp_frontier_req_client : public std::enable_shared_from_this<tcp_frontier_req_client>
{
public:
    tcp_frontier_req_client (std::shared_ptr<germ::tcp_bootstrap_client>);
    ~tcp_frontier_req_client ();
    void run ();
    void receive_frontier ();
    void received_frontier (boost::system::error_code const &, size_t);
    void request_account (germ::account const &, germ::block_hash const &);
    void unsynced (MDB_txn *, germ::block_hash const &, germ::block_hash const &);
    void next (MDB_txn *);
    void insert_pull (germ::pull_info const &);
    std::shared_ptr<germ::tcp_bootstrap_client> connection;
    germ::account current;
    germ::account_info info;
    unsigned count;
    germ::account landing;
    germ::account faucet;
    std::chrono::steady_clock::time_point start_time;
    std::promise<bool> promise;
    /** A very rough estimate of the cost of `bulk_push`ing missing blocks */
    uint64_t bulk_push_cost;
};


}


#endif //SRC_FRONTIER_REQ_CLIENT_H
