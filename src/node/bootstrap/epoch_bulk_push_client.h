//
// Created by daoful on 18-7-31.
//

#ifndef SRC_EPOCH_BULK_PUSH_CLIENT_H
#define SRC_EPOCH_BULK_PUSH_CLIENT_H


namespace germ
{
class node;
class tcp_bootstrap_client;
class tcp_epoch_bulk_push_client : public std::enable_shared_from_this<tcp_epoch_bulk_push_client>
{
public:
    tcp_epoch_bulk_push_client (std::shared_ptr<germ::tcp_bootstrap_client> const &);
    ~tcp_epoch_bulk_push_client ();
    void start ();
    void push (MDB_txn *);
    void push_block (germ::epoch const &);
    void send_finished ();
    std::shared_ptr<germ::tcp_bootstrap_client> connection;
    std::promise<bool> promise;
    std::pair<germ::epoch_hash, germ::epoch_hash> current_target;
};


}

#endif //SRC_EPOCH_BULK_PUSH_CLIENT_H
