//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BULK_PULL_SERVER_H
#define SRC_BULK_PULL_SERVER_H

#include <src/node/common.hpp>


namespace germ
{
class node;
class tcp_bootstrap_server;
class tcp_bulk_pull_server : public std::enable_shared_from_this<tcp_bulk_pull_server>
{
public:
    tcp_bulk_pull_server (std::shared_ptr<germ::tcp_bootstrap_server> const &, std::unique_ptr<germ::bulk_pull>);
    void set_current_end ();
    std::unique_ptr<germ::tx> get_next ();
    void send_next ();
    void sent_action (boost::system::error_code const &, size_t);
    void send_finished ();
    void no_block_sent (boost::system::error_code const &, size_t);
    std::shared_ptr<germ::tcp_bootstrap_server> connection;
    std::unique_ptr<germ::bulk_pull> request;
    std::shared_ptr<std::vector<uint8_t>> send_buffer;
    germ::block_hash current;
};

}


#endif //SRC_BULK_PULL_SERVER_H
