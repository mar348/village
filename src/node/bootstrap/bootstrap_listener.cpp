//
// Created by daoful on 18-8-2.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bootstrap_listener.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/socket.h>

germ::tcp_bootstrap_listener::tcp_bootstrap_listener (boost::asio::io_service & service_a, uint16_t port_a, germ::node & node_a) :
        acceptor (service_a),
        local (boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v6::any (), port_a)),
        service (service_a),
        node (node_a)
{
}

void germ::tcp_bootstrap_listener::start ()
{
    acceptor.open (local.protocol ());
    acceptor.set_option (boost::asio::ip::tcp::acceptor::reuse_address (true));

    boost::system::error_code ec;
    acceptor.bind (local, ec);
    if (ec)
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Error while binding for bootstrap on port %1%: %2%") % local.port () % ec.message ());
        throw std::runtime_error (ec.message ());
    }

    acceptor.listen ();
    accept_connection ();
}

void germ::tcp_bootstrap_listener::stop ()
{
    decltype (connections) connections_l;
    {
        std::lock_guard<std::mutex> lock (mutex);
        on = false;
        connections_l.swap (connections);
    }
    acceptor.close ();
    for (auto & i : connections_l)
    {
        auto connection (i.second.lock ());
        if (connection)
        {
            connection->socket->close ();
        }
    }
}

void germ::tcp_bootstrap_listener::accept_connection ()
{
    auto socket (std::make_shared<germ::tcp_socket> (node.shared ()));
    acceptor.async_accept (socket->socket_m, [this, socket](boost::system::error_code const & ec) {
        accept_action (ec, socket);
    });
}

void germ::tcp_bootstrap_listener::accept_action (boost::system::error_code const & ec, std::shared_ptr<germ::tcp_socket> socket_a)
{
    if (!ec)
    {
        accept_connection ();
        auto connection (std::make_shared<germ::tcp_bootstrap_server> (socket_a, node.shared ()));
        {
            std::lock_guard<std::mutex> lock (mutex);
            if (connections.size () < node.config.bootstrap_connections_max && acceptor.is_open ())
            {
                connections[connection.get ()] = connection;
                connection->receive ();
            }
        }
    }
    else
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Error while accepting bootstrap connections: %1%") % ec.message ());
    }
}

boost::asio::ip::tcp::endpoint germ::tcp_bootstrap_listener::endpoint ()
{
    return boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v6::loopback (), local.port ());
}
