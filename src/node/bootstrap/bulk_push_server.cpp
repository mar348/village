//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bulk_push_server.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/socket.h>
#include <src/lib/tx.h>


germ::tcp_bulk_push_server::tcp_bulk_push_server (std::shared_ptr<germ::tcp_bootstrap_server> const & connection_a) :
        receive_buffer (std::make_shared<std::vector<uint8_t>> ()),
        connection (connection_a)
{
    receive_buffer->resize (512);
}

void germ::tcp_bulk_push_server::receive ()
{
    auto this_l (shared_from_this ());
    connection->socket->async_read (receive_buffer, 1 + sizeof(size_t), [this_l](boost::system::error_code const & ec, size_t size_a) {
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

void germ::tcp_bulk_push_server::received_type ()
{
    auto this_l (shared_from_this ());
    germ::block_type type (static_cast<germ::block_type> (receive_buffer->data ()[0]));
    size_t size (*reinterpret_cast<size_t*> (&receive_buffer->data ()[1]));
    switch (type)
    {
        case germ::block_type::send:
        {
            connection->node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::send, germ::stat::dir::in);
            connection->socket->async_read (receive_buffer, size, [this_l, type](boost::system::error_code const & ec, size_t size_a) {
                this_l->received_block (ec, size_a, type);
            });
            break;
        }
        case germ::block_type::receive:
        {
            connection->node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::receive, germ::stat::dir::in);
            connection->socket->async_read (receive_buffer, size, [this_l, type](boost::system::error_code const & ec, size_t size_a) {
                this_l->received_block (ec, size_a, type);
            });
            break;
        }
        case germ::block_type::open:
        {
            connection->node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::open, germ::stat::dir::in);
            connection->socket->async_read (receive_buffer, size, [this_l, type](boost::system::error_code const & ec, size_t size_a) {
                this_l->received_block (ec, size_a, type);
            });
            break;
        }
        case germ::block_type::vote:
        {
            connection->node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::vote, germ::stat::dir::in);
            connection->socket->async_read (receive_buffer, size, [this_l, type](boost::system::error_code const & ec, size_t size_a) {
                this_l->received_block (ec, size_a, type);
            });
            break;
        }
        case germ::block_type::state:
        {
            connection->node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::state_block, germ::stat::dir::in);
            connection->socket->async_read (receive_buffer, size, [this_l, type](boost::system::error_code const & ec, size_t size_a) {
                this_l->received_block (ec, size_a, type);
            });
            break;
        }
        //TODO: integrate epoch into this
//        case germ::block_type::epoch:
//        {
//
//        }
        case germ::block_type::not_a_block:
        {
            connection->finish_request ();
            break;
        }
        default:
        {
            if (connection->node->config.logging.network_packet_logging ())
            {
                BOOST_LOG (connection->node->log) << "Unknown type received as block type";
            }
            break;
        }
    }
}

void germ::tcp_bulk_push_server::received_block (boost::system::error_code const & ec, size_t size_a, germ::block_type type_a)
{
    if (!ec)
    {
        germ::bufferstream stream (receive_buffer->data (), size_a);
        auto block (germ::deserialize_block (stream, type_a));
        if (block != nullptr /*&& !germ::work_validate (*block)*/ )
        {
            connection->node->process_active (std::move (block));
            receive ();
        }
        else
        {
            if (connection->node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (connection->node->log) << "Error deserializing block received from pull request";
            }
        }
    }
}
