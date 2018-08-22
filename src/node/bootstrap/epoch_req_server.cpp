//
// Created by daoful on 18-7-30.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/epoch_req_server.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/socket.h>


germ::tcp_epoch_req_server::tcp_epoch_req_server (std::shared_ptr<germ::tcp_bootstrap_server> const & connection_a, std::unique_ptr<germ::epoch_req> request_a) :
connection (connection_a),
//current (request_a->start.number() - 1),
info (0, 0, 0),
request (std::move (request_a)),
send_buffer (std::make_shared<std::vector<uint8_t>> ())
{
    next ();
    skip_old ();
}

void germ::tcp_epoch_req_server::skip_old ()
{
    if (request->age == std::numeric_limits<decltype (request->age)>::max ())
        return;

    auto now (germ::seconds_since_epoch ());
    while ( (now - info.modified) >= request->age )
    {
        next ();
    }
}

void germ::tcp_epoch_req_server::send_next ()
{
    {
        send_buffer->clear ();
        germ::vectorstream stream (*send_buffer);
        write (stream, info.head.bytes);
    }
    auto this_l (shared_from_this ());
    if (connection->node->config.logging.epoch_bulk_pull_logging ())
    {
        BOOST_LOG (connection->node->log) << boost::str (boost::format ("Sending epoch for %1%") % info.head.to_string ());
    }
    next ();
    connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
        this_l->sent_action (ec, size_a);
    });

    send_finished ();
}

void germ::tcp_epoch_req_server::send_finished ()
{
    {
        send_buffer->clear ();
        germ::vectorstream stream (*send_buffer);
        germ::uint256_union zero (0);
//        write (stream, zero.bytes);
        write (stream, zero.bytes);
    }
    auto this_l (shared_from_this ());
    if (connection->node->config.logging.network_logging ())
    {
        BOOST_LOG (connection->node->log) << "epoch sending finished";
    }
    connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
        this_l->no_block_sent (ec, size_a);
    });
}

void germ::tcp_epoch_req_server::no_block_sent (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        connection->finish_request ();
    }
    else
    {
        if (connection->node->config.logging.network_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Error sending epoch finish: %1%") % ec.message ());
        }
    }
}

void germ::tcp_epoch_req_server::sent_action (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        send_next ();
    }
    else
    {
        if (connection->node->config.logging.network_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Error sending epoch pair: %1%") % ec.message ());
        }
    }
}

void germ::tcp_epoch_req_server::next ()
{
    germ::transaction transaction (connection->node->epoch_store.environment, nullptr, false);
    auto iterator (connection->node->epoch_store.latest_begin (transaction, info.head.number () + 1));
    if (iterator != connection->node->epoch_store.latest_end ())
    {
//        current = germ::uint256_union (iterator->first.uint256 ());
        info = germ::epoch_info (iterator->second);
    }
    else
    {
//        current.clear ();
        info = germ::epoch_info (0, 0, 0);
    }
}



