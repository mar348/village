//
// Created by daoful on 18-7-26.
//

#ifndef SRC_SOCKET_H
#define SRC_SOCKET_H

#include <src/node/common.hpp>

namespace germ
{
class node;
class tcp_socket : public std::enable_shared_from_this<germ::tcp_socket>
{
public:
    tcp_socket (std::shared_ptr<germ::node>);
    void async_connect (germ::tcp_endpoint const &, std::function<void(boost::system::error_code const &)>);
    void async_read (std::shared_ptr<std::vector<uint8_t>>, size_t, std::function<void(boost::system::error_code const &, size_t)>);
    void async_write (std::shared_ptr<std::vector<uint8_t>>, std::function<void(boost::system::error_code const &, size_t)>);
    void start (std::chrono::steady_clock::time_point = std::chrono::steady_clock::now () + std::chrono::seconds (5));
    void stop ();
    void close ();
    germ::tcp_endpoint remote_endpoint ();
    boost::asio::ip::tcp::socket socket_m;

private:
    std::atomic<unsigned> ticket;
    std::shared_ptr<germ::node> node;
};

}


#endif //SRC_SOCKET_H
