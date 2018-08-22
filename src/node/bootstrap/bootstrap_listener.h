//
// Created by daoful on 18-8-2.
//

#ifndef SRC_BOOTSTRAP_LISTENER_H
#define SRC_BOOTSTRAP_LISTENER_H


namespace germ
{
class node;
class tcp_socket;
class tcp_bootstrap_server;
class tcp_bootstrap_listener : public std::enable_shared_from_this<tcp_bootstrap_listener>
{
public:
    tcp_bootstrap_listener (boost::asio::io_service &, uint16_t, germ::node &);
    void start ();
    void stop ();
    void accept_connection ();
    void accept_action (boost::system::error_code const &, std::shared_ptr<germ::tcp_socket>);
    std::mutex mutex;
    std::unordered_map<germ::tcp_bootstrap_server *, std::weak_ptr<germ::tcp_bootstrap_server>> connections;
    germ::tcp_endpoint endpoint ();
    boost::asio::ip::tcp::acceptor acceptor;
    germ::tcp_endpoint local;
    boost::asio::io_service & service;
    germ::node & node;
    bool on;
};

}


#endif //SRC_BOOTSTRAP_LISTENER_H
