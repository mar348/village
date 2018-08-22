//
// Created by daoful on 18-7-31.
//

#ifndef SRC_EPOCH_REQ_CLIENT_H
#define SRC_EPOCH_REQ_CLIENT_H


#include <src/node/bootstrap/bootstrap.h>

namespace germ
{
class node;
class tcp_bootstrap_client;
class tcp_epoch_req_client : public std::enable_shared_from_this<germ::tcp_epoch_req_client>
{
public:
    tcp_epoch_req_client (std::shared_ptr<germ::tcp_bootstrap_client>);
    ~tcp_epoch_req_client ();
    void run ();
    void receive_epoch();
    void received_epoch(boost::system::error_code const &, size_t);
    void request_account (germ::account const &, germ::block_hash const &);
    void unsynced (MDB_txn *, germ::block_hash const &, germ::block_hash const &);
    void next (MDB_txn *);
    void insert_pull (germ::pull_info const &);
    std::shared_ptr<germ::tcp_bootstrap_client> connection;
//    germ::account current;
    germ::epoch_info info;
    unsigned count;
    germ::account landing;
    germ::account faucet;
    std::chrono::steady_clock::time_point start_time;
    std::promise<bool> promise;
    /** A very rough estimate of the cost of `bulk_push`ing missing blocks */
    uint64_t epoch_bulk_push_cost;
};


}


#endif //SRC_EPOCH_REQ_CLIENT_H
