//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BULK_PULL_BLOCKS_SERVER_H
#define SRC_BULK_PULL_BLOCKS_SERVER_H

#include <src/node/common.hpp>



namespace germ
{
class node;
class tcp_bootstrap_server;
class tcp_bulk_pull_blocks_server : public std::enable_shared_from_this<tcp_bulk_pull_blocks_server>
{
public:
    tcp_bulk_pull_blocks_server(std::shared_ptr<germ::tcp_bootstrap_server> const &, std::unique_ptr<germ::bulk_pull_blocks>);
    void set_params();
    std::unique_ptr<germ::tx> get_next();
    void send_next();
    void sent_action(boost::system::error_code const &, size_t);
    void send_finished();
    void no_block_sent(boost::system::error_code const &, size_t);

    std::shared_ptr<germ::tcp_bootstrap_server> connection;
    std::unique_ptr<germ::bulk_pull_blocks> request;
    std::shared_ptr<std::vector<uint8_t>> send_buffer;
    germ::store_iterator stream;
    germ::transaction stream_transaction;
    uint32_t sent_count;
    germ::block_hash checksum;
};

}


#endif //SRC_BULK_PULL_BLOCKS_SERVER_H
