//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bulk_pull_server.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/socket.h>


/**
 * Handle a request for the pull of all blocks associated with an account
 * The account is supplied as the "start" member, and the final block to
 * send is the "end" member
 */
void germ::tcp_bulk_pull_server::set_current_end ()
{
    assert (request != nullptr);
    germ::transaction transaction (connection->node->store.environment, nullptr, false);
    if (!connection->node->store.block_exists (transaction, request->end))
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Bulk pull end block doesn't exist: %1%, sending everything") % request->end.to_string ());
        }
        request->end.clear ();
    }
    germ::account_info info;
    auto found (connection->node->store.account_get (transaction, request->start, info));
    if (!found)
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Request for unknown account: %1%") % request->start.to_account ());
        }
        current = request->end;
    }
    else
    {
        if (!request->end.is_zero ())
        {
            auto account (connection->node->ledger.account (transaction, request->end));
            if (account == request->start)
            {
                current = info.head;
            }
            else
            {
                current = request->end;
            }
        }
        else
        {
            current = info.head;
        }
    }
}

void germ::tcp_bulk_pull_server::send_next ()
{
    std::unique_ptr<germ::tx> block (get_next ());
    if (block != nullptr)
    {
        {
            send_buffer->clear ();
            germ::vectorstream stream (*send_buffer);
            germ::serialize_block (stream, *block);
        }
        auto this_l (shared_from_this ());
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Sending block: %1%") % block->hash ().to_string ());
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

std::unique_ptr<germ::tx> germ::tcp_bulk_pull_server::get_next ()
{
    std::unique_ptr<germ::tx> result;
    if (current != request->end)
    {
        germ::transaction transaction (connection->node->store.environment, nullptr, false);
        result = connection->node->store.block_get (transaction, current);
        if (result != nullptr)
        {
            auto previous (result->previous ());
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

void germ::tcp_bulk_pull_server::sent_action (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        send_next ();
    }
    else
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Unable to bulk send block: %1%") % ec.message ());
        }
    }
}

void germ::tcp_bulk_pull_server::send_finished ()
{
    send_buffer->clear ();
    send_buffer->push_back (static_cast<uint8_t> (germ::block_type::not_a_block));
    for (int i = 0; i < 8; ++i) {
        send_buffer->push_back(0);
    }
    
    auto this_l (shared_from_this ());
    if (connection->node->config.logging.bulk_pull_logging ())
    {
        BOOST_LOG (connection->node->log) << "Bulk sending finished";
    }
    connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
        this_l->no_block_sent (ec, size_a);
    });
}

void germ::tcp_bulk_pull_server::no_block_sent (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        assert (size_a == 1 + sizeof(size_t));
        connection->finish_request ();
    }
    else
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << "Unable to send not-a-block";
        }
    }
}

germ::tcp_bulk_pull_server::tcp_bulk_pull_server (std::shared_ptr<germ::tcp_bootstrap_server> const & connection_a, std::unique_ptr<germ::bulk_pull> request_a) :
        connection (connection_a),
        request (std::move (request_a)),
        send_buffer (std::make_shared<std::vector<uint8_t>> ())
{
    set_current_end ();
}
