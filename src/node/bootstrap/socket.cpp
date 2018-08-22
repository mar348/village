//
// Created by daoful on 18-7-26.
//

#include <src/node/bootstrap/socket.h>
#include <src/node/node.hpp>

germ::tcp_socket::tcp_socket (std::shared_ptr<germ::node> node_a) :
        socket_m (node_a->service),
        ticket (0),
        node (node_a)
{
}

void germ::tcp_socket::async_connect (germ::tcp_endpoint const & endpoint_a, std::function<void(boost::system::error_code const &)> callback_a)
{
    auto this_l (shared_from_this ());
    start ();
    socket_m.async_connect (endpoint_a, [this_l, callback_a](boost::system::error_code const & ec) {
        this_l->stop ();
        callback_a (ec);
    });
}

void germ::tcp_socket::async_read (std::shared_ptr<std::vector<uint8_t>> buffer_a, size_t size_a, std::function<void(boost::system::error_code const &, size_t)> callback_a)
{
    assert (size_a <= buffer_a->size ());
    auto this_l (shared_from_this ());
    start ();
    boost::asio::async_read (socket_m, boost::asio::buffer (buffer_a->data (), size_a), [this_l, callback_a](boost::system::error_code const & ec, size_t size_a) {
        this_l->stop ();
        callback_a (ec, size_a);
    });
}

void germ::tcp_socket::async_write (std::shared_ptr<std::vector<uint8_t>> buffer_a, std::function<void(boost::system::error_code const &, size_t)> callback_a)
{
    auto this_l (shared_from_this ());
    start ();
    boost::asio::async_write (socket_m, boost::asio::buffer (buffer_a->data (), buffer_a->size ()), [this_l, callback_a](boost::system::error_code const & ec, size_t size_a) {
        this_l->stop ();
        callback_a (ec, size_a);
    });
}

void germ::tcp_socket::start (std::chrono::steady_clock::time_point timeout_a)
{
    auto ticket_l (++ticket);
    std::weak_ptr<germ::tcp_socket> this_w (shared_from_this ());
    node->alarm.add (timeout_a, [this_w, ticket_l]() {
        if (auto this_l = this_w.lock ())
        {
            if (this_l->ticket == ticket_l)
            {
                this_l->socket_m.close ();
                if (this_l->node->config.logging.bulk_pull_logging ())
                {
          //          BOOST_LOG (this_l->node->log) << boost::str (boost::format ("Disconnecting from %1% due to timeout") % this_l->socket_m.remote_endpoint ());
                }

            }
        }
    });
}

void germ::tcp_socket::stop ()
{
    ++ticket;
}

void germ::tcp_socket::close ()
{
    socket_m.close ();
}

germ::tcp_endpoint germ::tcp_socket::remote_endpoint ()
{
    return socket_m.remote_endpoint ();
}
