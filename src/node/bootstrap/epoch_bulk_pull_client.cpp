//
// Created by daoful on 18-7-31.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/epoch_bulk_pull_client.h>
#include <src/node/bootstrap/bootstrap_client.h>
#include <src/node/bootstrap/bootstrap_attempt.h>
#include <src/node/bootstrap/socket.h>


germ::tcp_epoch_bulk_pull_client::tcp_epoch_bulk_pull_client (std::shared_ptr<germ::tcp_bootstrap_client> connection_a, germ::pull_info const & pull_a) :
connection (connection_a),
pull (pull_a)
{
    std::lock_guard<std::mutex> mutex (connection->attempt->mutex);
    ++connection->attempt->pulling;
    connection->attempt->condition.notify_all ();
}

germ::tcp_epoch_bulk_pull_client::~tcp_epoch_bulk_pull_client ()
{
    // If received end block is not expected end block
    if (expected != pull.end)
    {
        pull.head = expected;
        connection->attempt->requeue_pull (pull);
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Bulk pull end block is not expected %1%") % pull.end.to_string ());
        }
    }
    std::lock_guard<std::mutex> mutex (connection->attempt->mutex);
    --connection->attempt->pulling;
    connection->attempt->condition.notify_all ();
}

void germ::tcp_epoch_bulk_pull_client::request ()
{
    expected = pull.head;
    germ::epoch_bulk_pull req;
    req.start = pull.head;
    req.end = pull.end;
    auto buffer (std::make_shared<std::vector<uint8_t>> ());
    {
        germ::vectorstream stream (*buffer);
        req.serialize (stream);
    }

    auto this_l (shared_from_this ());
    connection->socket->async_write (buffer, [this_l, buffer](boost::system::error_code const & ec, size_t size_a) {
        if (!ec)
        {
            this_l->receive_block ();
        }
        else
        {
            if (this_l->connection->node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (this_l->connection->node->log) << boost::str (boost::format ("Error sending epoch bulk pull request to %1%: to %2%") % ec.message () % this_l->connection->endpoint);
            }
        }
    });
}

void germ::tcp_epoch_bulk_pull_client::receive_block ()
{
    auto this_l (shared_from_this ());
    connection->socket->async_read (connection->receive_buffer, 1, [this_l](boost::system::error_code const & ec, size_t size_a) {
        if (!ec)
        {
            this_l->received_type ();
        }
        else
        {
            if (this_l->connection->node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (this_l->connection->node->log) << boost::str (boost::format ("Error receiving block type: %1%") % ec.message ());
            }
        }
    });
}

void germ::tcp_epoch_bulk_pull_client::received_type ()
{
    auto this_l (shared_from_this ());
    germ::block_type type (static_cast<germ::block_type> (connection->receive_buffer->data ()[0]));
    //size_t size (static_cast<size_t> (connection->receive_buffer->data ()[1]));
    //take the data that's located at data[1], and take 8 bytes of it (sizeof(size_t))
    size_t body_size (*reinterpret_cast<size_t*> (&(connection->receive_buffer->data ()[1])));
    switch (type)
    {
        case germ::block_type::epoch:
        {
            connection->socket->async_read (connection->receive_buffer, body_size, [this_l, type](boost::system::error_code const & ec, size_t size_a) {
                this_l->received_block (ec, size_a, type);
            });
            break;
        }
        case germ::block_type::not_an_epoch:
        {
            // Avoid re-using slow peers, or peers that sent the wrong blocks.
            if (!connection->pending_stop && expected == pull.end)
            {
                connection->attempt->pool_connection (connection);
            }
            break;
        }
        default:
        {
            if (connection->node->config.logging.network_packet_logging ())
            {
                BOOST_LOG (connection->node->log) << boost::str (boost::format ("Unknown type received as block type: %1%") % static_cast<int> (type));
            }
            break;
        }
    }
}

void germ::tcp_epoch_bulk_pull_client::received_block (boost::system::error_code const & ec, size_t size_a, germ::block_type type_a)
{
    if (!ec)
    {
        germ::bufferstream stream (connection->receive_buffer->data (), size_a);
        std::shared_ptr<germ::epoch> block (germ::deserialize_epoch (stream, type_a));
        if (block != nullptr /*&& germ::work_validate (*block)*/ )
        {
            auto hash (block->hash ());
            if (connection->node->config.logging.bulk_pull_logging ())
            {
                std::string block_l;
                block->serialize_json (block_l);
                BOOST_LOG (connection->node->log) << boost::str (boost::format ("Pulled block %1% %2%") % hash.to_string () % block_l);
            }
            if (hash == expected)
            {
                expected = block->previous_epoch ();
            }
            if (connection->block_count++ == 0)
            {
                connection->start_time = std::chrono::steady_clock::now ();
            }
            connection->attempt->total_blocks++;
//            connection->attempt->node->block_processor.add (block, std::chrono::steady_clock::time_point ());
            if (!connection->hard_stop.load ())
            {
                receive_block ();
            }
        }
        else
        {
            if (connection->node->config.logging.epoch_bulk_pull_logging ())
            {
                BOOST_LOG (connection->node->log) << "Error deserializing block received from epoch pull request";
            }
        }
    }
    else
    {
        if (connection->node->config.logging.epoch_bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Error epoch bulk receiving block: %1%") % ec.message ());
        }
    }
}


