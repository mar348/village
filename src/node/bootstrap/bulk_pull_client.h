//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BULK_PULL_CLIENT_H
#define SRC_BULK_PULL_CLIENT_H

#include <src/node/common.hpp>
#include <src/node/bootstrap/bootstrap.h>


namespace germ
{
class node;
class tcp_bootstrap_client;
class tcp_bulk_pull_client : public std::enable_shared_from_this<tcp_bulk_pull_client>
{
public:
    tcp_bulk_pull_client (std::shared_ptr<germ::tcp_bootstrap_client>, germ::pull_info const &);
    ~tcp_bulk_pull_client ();
    void request ();
    void receive_block ();
    void received_type ();
    void received_block (boost::system::error_code const &, size_t, germ::block_type);
    germ::block_hash first ();
    std::shared_ptr<germ::tcp_bootstrap_client> connection;
    germ::block_hash expected;
    germ::pull_info pull;
};

}


#endif //SRC_BULK_PULL_CLIENT_H
