//
// Created by daoful on 18-7-30.
//

#ifndef SRC_EPOCH_TRANSACTION_SERVER_H
#define SRC_EPOCH_TRANSACTION_SERVER_H

#include <src/node/common.hpp>

namespace germ
{
class node;
class tcp_bootstrap_server;
class tcp_epoch_req_server : public std::enable_shared_from_this<tcp_epoch_req_server>
{
public:
    tcp_epoch_req_server (std::shared_ptr<germ::tcp_bootstrap_server> const &, std::unique_ptr<germ::epoch_req>);
    void skip_old ();
    void send_next ();
    void sent_action (boost::system::error_code const &, size_t);
    void send_finished ();
    void no_block_sent (boost::system::error_code const &, size_t);
    void next ();
    std::shared_ptr<germ::tcp_bootstrap_server> connection;
//    germ::epoch_hash current;
    germ::epoch_info info;
    std::unique_ptr<germ::epoch_req> request;
    std::shared_ptr<std::vector<uint8_t>> send_buffer;
    size_t count;
};


}


#endif //SRC_EPOCH_TRANSACTION_SERVER_H
