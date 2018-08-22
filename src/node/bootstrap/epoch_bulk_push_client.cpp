//
// Created by daoful on 18-7-31.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/epoch_bulk_push_client.h>
#include <src/node/bootstrap/bootstrap_client.h>
#include <src/node/bootstrap/bootstrap_attempt.h>
#include <src/node/bootstrap/socket.h>


germ::tcp_epoch_bulk_push_client::tcp_epoch_bulk_push_client (std::shared_ptr<germ::tcp_bootstrap_client> const & connection_a) :
connection (connection_a)
{
}

germ::tcp_epoch_bulk_push_client::~tcp_epoch_bulk_push_client ()
{
}

void germ::tcp_epoch_bulk_push_client::start ()
{
    germ::epoch_bulk_push message;
    auto buffer (std::make_shared<std::vector<uint8_t>> ());
    {
        germ::vectorstream stream (*buffer);
        message.serialize (stream);
    }
    auto this_l (shared_from_this ());
    connection->socket->async_write (buffer, [this_l, buffer](boost::system::error_code const & ec, size_t size_a) {
        germ::transaction transaction (this_l->connection->node->epoch_store.environment, nullptr, false);
        if (!ec)
        {
            this_l->push (transaction);
        }
        else
        {
            if (this_l->connection->node->config.logging.epoch_bulk_pull_logging ())
            {
                BOOST_LOG (this_l->connection->node->log) << boost::str (boost::format ("Unable to send epoch bulk_push request: %1%") % ec.message ());
            }
        }
    });
}

void germ::tcp_epoch_bulk_push_client::push (MDB_txn * transaction_a)
{
    std::unique_ptr<germ::epoch> block;
    bool finished (false);
    while (block == nullptr && !finished)
    {
        if (current_target.first.is_zero () || current_target.first == current_target.second)
        {
            std::lock_guard<std::mutex> guard (connection->attempt->mutex);
            if (!connection->attempt->bulk_push_targets.empty ())
            {
                current_target = connection->attempt->bulk_push_targets.back ();
                connection->attempt->bulk_push_targets.pop_back ();
            }
            else
            {
                finished = true;
            }
        }
        if (!finished)
        {
            block = connection->node->epoch_store.block_get (transaction_a, current_target.first);
            if (block == nullptr)
            {
                current_target.first = germ::epoch_hash(0);
            }
            else
            {
                if (connection->node->config.logging.epoch_bulk_pull_logging ())
                {
                    BOOST_LOG (connection->node->log) << "Epoch bulk pushing range " << current_target.first.to_string () << " down to " << current_target.second.to_string ();
                }
            }
        }
    }
    if (finished)
    {
        send_finished ();
    }
    else
    {
        current_target.first = block->previous_epoch ();
        push_block (*block);
    }
}

void germ::tcp_epoch_bulk_push_client::send_finished ()
{
    auto buffer (std::make_shared<std::vector<uint8_t>> ());
    buffer->push_back (static_cast<uint8_t> (germ::block_type::not_an_epoch));
    connection->node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::epoch_bulk_push, germ::stat::dir::out);
    if (connection->node->config.logging.network_logging ())
    {
            BOOST_LOG (connection->node->log) << "Epoch bulk push finished";
    }
    auto this_l (shared_from_this ());
    connection->socket->async_write (buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
        try
        {
            this_l->promise.set_value (true);
        }
        catch (std::future_error &)
        {
        }
    });
}

void germ::tcp_epoch_bulk_push_client::push_block (germ::epoch const & block_a)
{
    auto buffer (std::make_shared<std::vector<uint8_t>> ());
    {
        germ::vectorstream stream (*buffer);
        germ::serialize_epoch (stream, block_a);
    }
    auto this_l (shared_from_this ());
    connection->socket->async_write (buffer, [this_l, buffer](boost::system::error_code const & ec, size_t size_a) {
        if (!ec)
        {
            germ::transaction transaction (this_l->connection->node->epoch_store.environment, nullptr, false);
            this_l->push (transaction);
        }
        else
        {
            if (this_l->connection->node->config.logging.epoch_bulk_pull_logging ())
            {
                BOOST_LOG (this_l->connection->node->log) << boost::str (boost::format ("Error sending block during epoch bulk push: %1%") % ec.message ());
            }
        }
    });
}
