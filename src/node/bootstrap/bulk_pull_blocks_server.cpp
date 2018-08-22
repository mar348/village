//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bulk_pull_blocks_server.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/socket.h>


/**
 * Bulk pull of a range of blocks, or a checksum for a range of
 * blocks [min_hash, max_hash) up to a max of max_count.  mode
 * specifies whether the list is returned or a single checksum
 * of all the hashes.  The checksum is computed by XORing the
 * hash of all the blocks that would be returned
 */
void germ::tcp_bulk_pull_blocks_server::set_params ()
{
    assert (request != nullptr);

    if (connection->node->config.logging.bulk_pull_logging ())
    {
        std::string modeName = "<unknown>";

        switch (request->mode)
        {
            case germ::bulk_pull_blocks_mode::list_blocks:
                modeName = "list";
                break;
            case germ::bulk_pull_blocks_mode::checksum_blocks:
                modeName = "checksum";
                break;
        }

        BOOST_LOG (connection->node->log) << boost::str (boost::format ("Bulk pull of block range starting, min (%1%) to max (%2%), max_count = %3%, mode = %4%") % request->min_hash.to_string () % request->max_hash.to_string () % request->max_count % modeName);
    }

    stream = connection->node->store.block_info_begin (stream_transaction, request->min_hash);

    if ( !(request->max_hash < request->min_hash) )
        return;

    if (connection->node->config.logging.bulk_pull_logging ())
    {
        BOOST_LOG (connection->node->log) << boost::str (boost::format ("Bulk pull of block range is invalid, min (%1%) is greater than max (%2%)") % request->min_hash.to_string () % request->max_hash.to_string ());
    }

    request->max_hash = request->min_hash;
}

void germ::tcp_bulk_pull_blocks_server::send_next ()
{
    std::unique_ptr<germ::tx> block (get_next ());
    if (block != nullptr)
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Sending block: %1%") % block->hash ().to_string ());
        }

        send_buffer->clear ();
        auto this_l (shared_from_this ());

        if (request->mode == germ::bulk_pull_blocks_mode::list_blocks)
        {
            germ::vectorstream stream (*send_buffer);
            germ::serialize_block (stream, *block);
        }
        else if (request->mode == germ::bulk_pull_blocks_mode::checksum_blocks)
        {
            checksum ^= block->hash ();
        }

        connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
            this_l->sent_action (ec, size_a);
        });
    }
    else
    {
        if (connection->node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Done sending blocks"));
        }

        if (request->mode == germ::bulk_pull_blocks_mode::checksum_blocks)
        {
            {
                send_buffer->clear ();
                germ::vectorstream stream (*send_buffer);
                write (stream, static_cast<uint8_t> (germ::block_type::not_a_block));
                write (stream, checksum);
            }

            auto this_l (shared_from_this ());
            if (connection->node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (connection->node->log) << boost::str (boost::format ("Sending checksum: %1%") % checksum.to_string ());
            }

            connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
                this_l->send_finished ();
            });
        }
        else
        {
            send_finished ();
        }
    }
}

std::unique_ptr<germ::tx> germ::tcp_bulk_pull_blocks_server::get_next ()
{
    std::unique_ptr<germ::tx> result;
    bool out_of_bounds;

    out_of_bounds = false;
    if (request->max_count != 0)
    {
        if (sent_count >= request->max_count)
        {
            out_of_bounds = true;
        }
        sent_count++;
    }

    if (out_of_bounds)
        return result;

    if (stream->first.size () == 0)
        return result;

    auto current = stream->first.uint256 ();
    if (current < request->max_hash)
    {
        germ::transaction transaction (connection->node->store.environment, nullptr, false);
        result = connection->node->store.block_get (transaction, current);

        ++stream;
    }
    return result;
}

void germ::tcp_bulk_pull_blocks_server::sent_action (boost::system::error_code const & ec, size_t size_a)
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

void germ::tcp_bulk_pull_blocks_server::send_finished ()
{
    send_buffer->clear ();
    send_buffer->push_back (static_cast<uint8_t> (germ::block_type::not_a_block));
    auto this_l (shared_from_this ());
    if (connection->node->config.logging.bulk_pull_logging ())
    {
        BOOST_LOG (connection->node->log) << "Bulk sending finished";
    }
    connection->socket->async_write (send_buffer, [this_l](boost::system::error_code const & ec, size_t size_a) {
        this_l->no_block_sent (ec, size_a);
    });
}

void germ::tcp_bulk_pull_blocks_server::no_block_sent (boost::system::error_code const & ec, size_t size_a)
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
            BOOST_LOG (connection->node->log) << "Unable to send not-a-block";
        }
    }
}

germ::tcp_bulk_pull_blocks_server::tcp_bulk_pull_blocks_server (std::shared_ptr<germ::tcp_bootstrap_server> const & connection_a, std::unique_ptr<germ::bulk_pull_blocks> request_a) :
        connection (connection_a),
        request (std::move (request_a)),
        send_buffer (std::make_shared<std::vector<uint8_t>> ()),
        stream (nullptr),
        stream_transaction (connection_a->node->store.environment, nullptr, false),
        sent_count (0),
        checksum (0)
{
    set_params ();
}

