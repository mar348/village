//
// Created by daoful on 18-7-26.
//

#ifndef SRC_BULK_PUSH_SERVER_H
#define SRC_BULK_PUSH_SERVER_H



namespace germ
{
class node;
class tcp_bootstrap_server;
class tcp_bulk_push_server : public std::enable_shared_from_this<tcp_bulk_push_server>
{
public:
    tcp_bulk_push_server (std::shared_ptr<germ::tcp_bootstrap_server> const &);
    void receive ();
    void receive_block ();
    void received_type ();
    void received_block (boost::system::error_code const &, size_t, germ::block_type);
    std::shared_ptr<std::vector<uint8_t>> receive_buffer;
    std::shared_ptr<germ::tcp_bootstrap_server> connection;
};


}


#endif //SRC_BULK_PUSH_SERVER_H
