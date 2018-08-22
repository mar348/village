//
// Created by daoful on 18-7-31.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/epoch_bulk_pull_server.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/socket.h>


/**
 * Handle a request for the pull of epoch blocks associated with an start and end.
 * 
 */
void germ::tcp_epoch_bulk_pull_server::set_current_end ()
{
    assert (request != nullptr);
    germ::transaction transaction (connection->node->epoch_store.environment, nullptr, false);
    if (!connection->node->epoch_store.block_exists (transaction, request->end))
    {
        if (connection->node->config.logging.epoch_bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Epoch bulk pull end block doesn't exist: %1%, sending everything") % request->end.to_string ());
        }
        request->end.clear ();
    }
    germ::epoch_info info;
    auto found (connection->node->epoch_store.block_get (transaction, request->start));
    if (!found)
    {
        current = request->end;
    }
    else
    {
        current = request->start;
    }
}

void germ::tcp_epoch_bulk_pull_server::send_next ()
{
    std::unique_ptr<germ::epoch> block (get_next ());
    if (block != nullptr)
    {
        {
            send_buffer->clear ();
            germ::vectorstream stream (*send_buffer);
            germ::serialize_epoch (stream, *block);
        }
        auto this_l (shared_from_this ());
        if (connection->node->config.logging.epoch_bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Sending epoch: %1%") % block->hash ().to_string ());
        }
        connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
            this_l->sent_action (ec, size_a);
        });
    }
    else
    {
        send_finished ();
    }
}

std::unique_ptr<germ::epoch> germ::tcp_epoch_bulk_pull_server::get_next ()
{
    std::unique_ptr<germ::epoch> result;
    if (current != request->end)
    {
        germ::transaction transaction (connection->node->epoch_store.environment, nullptr, false);
        result = connection->node->epoch_store.block_get (transaction, current);
        if (result != nullptr)
        {
            auto previous (result->previous_epoch ());
            if (!previous.is_zero ())
            {
                current = previous;
            }
            else
            {
                current = request->end;
            }
        }
        else
        {
            current = request->end;
        }
    }
    return result;
}

void germ::tcp_epoch_bulk_pull_server::sent_action (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        send_next ();
    }
    else
    {
        if (connection->node->config.logging.epoch_bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Unable to bulk send epoch: %1%") % ec.message ());
        }
    }
}

void germ::tcp_epoch_bulk_pull_server::send_finished ()
{
    send_buffer->clear ();
    send_buffer->push_back (static_cast<uint8_t> (germ::block_type::not_an_epoch));
    auto this_l (shared_from_this ());
    if (connection->node->config.logging.epoch_bulk_pull_logging ())
    {
        BOOST_LOG (connection->node->log) << "Bulk sending finished";
    }
    connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
        this_l->no_block_sent (ec, size_a);
    });
}

void germ::tcp_epoch_bulk_pull_server::no_block_sent (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        assert (size_a == 1);
        connection->finish_request ();
    }
    else
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << "Unable to send not-a-epoch";
        }
    }
}

germ::tcp_epoch_bulk_pull_server::tcp_epoch_bulk_pull_server (std::shared_ptr<germ::tcp_bootstrap_server> const & connection_a, std::unique_ptr<germ::epoch_bulk_pull> request_a) :
        connection (connection_a),
        request (std::move (request_a)),
        send_buffer (std::make_shared<std::vector<uint8_t>> ())
{
    set_current_end ();
}