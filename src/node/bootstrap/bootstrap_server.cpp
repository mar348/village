//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/socket.h>
#include <src/node/bootstrap/bootstrap_server.h>
#include <src/node/bootstrap/bulk_pull_server.h>
#include <src/node/bootstrap/bulk_pull_blocks_server.h>
#include <src/node/bootstrap/bulk_push_server.h>
#include <src/node/bootstrap/frontier_req_server.h>
#include <src/node/bootstrap/epoch_bulk_pull_server.h>
#include <src/node/bootstrap/epoch_req_server.h>
#include <src/node/bootstrap/epoch_bulk_push_server.h>


germ::tcp_bootstrap_server::tcp_bootstrap_server (std::shared_ptr<germ::tcp_socket> socket_a, std::shared_ptr<germ::node> node_a) :
        receive_buffer (std::make_shared<std::vector<uint8_t>> ()),
        socket (socket_a),
        node (node_a)
{
    receive_buffer->resize (512);
}

germ::tcp_bootstrap_server::~tcp_bootstrap_server()
{
}

void germ::tcp_bootstrap_server::receive ()
{
    auto this_l (shared_from_this ());
    
    socket->async_read (receive_buffer, 8+6, [this_l](boost::system::error_code const & ec, size_t size_a) {
            this_l->receive_header_action (ec, size_a);
        });
}

void germ::tcp_bootstrap_server::receive_header_action (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        assert (size_a == (8+6));
        germ::bufferstream type_stream (receive_buffer->data (), size_a);
        auto error (false);
        germ::message_header header (error, type_stream);
        if (!error)
        {
            switch (header.type)
            {
                case germ::message_type::bulk_pull:
                {
                    node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::bulk_pull, germ::stat::dir::in);
                    auto this_l (shared_from_this ());
                    socket->async_read (receive_buffer, sizeof (germ::uint256_union) + sizeof (germ::uint256_union), [this_l, header](boost::system::error_code const & ec, size_t size_a) {
                        this_l->receive_bulk_pull_action (ec, size_a, header);
                    });
                    break;
                }
                case germ::message_type::bulk_pull_blocks:
                {
                    node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::bulk_pull_blocks, germ::stat::dir::in);
                    auto this_l (shared_from_this ());
                    socket->async_read (receive_buffer, sizeof (germ::uint256_union) + sizeof (germ::uint256_union) + sizeof (bulk_pull_blocks_mode) + sizeof (uint32_t), [this_l, header](boost::system::error_code const & ec, size_t size_a) {
                        this_l->receive_bulk_pull_blocks_action (ec, size_a, header);
                    });
                    break;
                }
                case germ::message_type::frontier_req:
                {
                    node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::frontier_req, germ::stat::dir::in);
                    auto this_l (shared_from_this ());
                    socket->async_read (receive_buffer, sizeof (germ::uint256_union) + sizeof (uint32_t) + sizeof (uint32_t), [this_l, header](boost::system::error_code const & ec, size_t size_a) {
                        this_l->receive_frontier_req_action (ec, size_a, header);
                    });
                    break;
                }
                case germ::message_type::bulk_push:
                {
                    node->stats.inc (germ::stat::type::bootstrap, germ::stat::detail::bulk_push, germ::stat::dir::in);
                    add_request (std::unique_ptr<germ::message> (new germ::bulk_push (header)));
                    break;
                }
                default:
                {
                    if (node->config.logging.network_logging ())
                    {
                        BOOST_LOG (node->log) << boost::str (boost::format ("Received invalid type from bootstrap connection %1%") % static_cast<uint8_t> (header.type));
                    }
                    break;
                }
            }
        }
    }
    else
    {
        if (node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (node->log) << boost::str (boost::format ("Error while receiving type: %1%") % ec.message ());
        }
    }
}

void germ::tcp_bootstrap_server::receive_bulk_pull_action (boost::system::error_code const & ec, size_t size_a, germ::message_header const & header_a)
{
    if (!ec)
    {
        auto error (false);
        germ::bufferstream stream (receive_buffer->data (), sizeof (germ::uint256_union) + sizeof (germ::uint256_union));
        std::unique_ptr<germ::bulk_pull> request (new germ::bulk_pull (error, stream, header_a));
        if (!error)
        {
            if (node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (node->log) << boost::str (boost::format ("Received bulk pull for %1% down to %2%") % request->start.to_string () % request->end.to_string ());
            }
            add_request (std::unique_ptr<germ::message> (request.release ()));
            receive ();
        }
    }
}

void germ::tcp_bootstrap_server::receive_bulk_pull_blocks_action (boost::system::error_code const & ec, size_t size_a, germ::message_header const & header_a)
{
    if (!ec)
    {
        auto error (false);
        germ::bufferstream stream (receive_buffer->data (), sizeof (germ::uint256_union) + sizeof (germ::uint256_union) + sizeof (bulk_pull_blocks_mode) + sizeof (uint32_t));
        std::unique_ptr<germ::bulk_pull_blocks> request (new germ::bulk_pull_blocks (error, stream, header_a));
        if (!error)
        {
            if (node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (node->log) << boost::str (boost::format ("Received bulk pull blocks for %1% to %2%") % request->min_hash.to_string () % request->max_hash.to_string ());
            }
            add_request (std::unique_ptr<germ::message> (request.release ()));
            receive ();
        }
    }
}

void germ::tcp_bootstrap_server::receive_frontier_req_action (boost::system::error_code const & ec, size_t size_a, germ::message_header const & header_a)
{
    if (!ec)
    {
        auto error (false);
        germ::bufferstream stream (receive_buffer->data (), sizeof (germ::uint256_union) + sizeof (uint32_t) + sizeof (uint32_t));
        std::unique_ptr<germ::frontier_req> request (new germ::frontier_req (error, stream, header_a));
        if (!error)
        {
            if (node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (node->log) << boost::str (boost::format ("Received frontier request for %1% with age %2%") % request->start.to_string () % request->age);
            }
            add_request (std::unique_ptr<germ::message> (request.release ()));
            receive ();
        }
    }
    else
    {
        if (node->config.logging.network_logging ())
        {
            BOOST_LOG (node->log) << boost::str (boost::format ("Error sending receiving frontier request: %1%") % ec.message ());
        }
    }
}

void germ::tcp_bootstrap_server::add_request (std::unique_ptr<germ::message> message_a)
{
    std::lock_guard<std::mutex> lock (mutex);
    auto start (requests.empty ());
    requests.push (std::move (message_a));
    if (start)
    {
        run_next ();
    }
}

void germ::tcp_bootstrap_server::finish_request ()
{
    std::lock_guard<std::mutex> lock (mutex);
    requests.pop ();
    if (!requests.empty ())
    {
        run_next ();
    }
}

namespace
{
    class request_response_visitor : public germ::message_visitor
    {
    public:
        request_response_visitor (std::shared_ptr<germ::tcp_bootstrap_server> connection_a) :
                connection (connection_a)
        {
        }
        virtual ~request_response_visitor () = default;
        void keepalive (germ::keepalive const &) override
        {
            assert (false);
        }
        void publish (germ::publish const &) override
        {
            assert (false);
        }
        void confirm_req (germ::confirm_req const &) override
        {
            assert (false);
        }
        void confirm_ack (germ::confirm_ack const &) override
        {
            assert (false);
        }
        void bulk_pull (germ::bulk_pull const &) override
        {
            auto response (std::make_shared<germ::tcp_bulk_pull_server> (connection, std::unique_ptr<germ::bulk_pull> (static_cast<germ::bulk_pull *> (connection->requests.front ().release ()))));
            response->send_next ();
        }
        void bulk_pull_blocks (germ::bulk_pull_blocks const &) override
        {
            auto response (std::make_shared<germ::tcp_bulk_pull_blocks_server> (connection, std::unique_ptr<germ::bulk_pull_blocks> (static_cast<germ::bulk_pull_blocks *> (connection->requests.front ().release ()))));
            response->send_next ();
        }
        void bulk_push (germ::bulk_push const &) override
        {
            auto response (std::make_shared<germ::tcp_bulk_push_server> (connection));
            response->receive ();
        }
        void frontier_req (germ::frontier_req const &) override
        {
            auto response (std::make_shared<germ::tcp_frontier_req_server> (connection, std::unique_ptr<germ::frontier_req> (static_cast<germ::frontier_req *> (connection->requests.front ().release ()))));
            response->send_next ();
        }
        void node_id_handshake (germ::node_id_handshake const &) override
        {
            assert (false);
        }
        void epoch_req (germ::epoch_req const & tx_r) override
        {
            auto response (std::make_shared<germ::tcp_epoch_req_server> (connection, std::unique_ptr<germ::epoch_req> (static_cast<germ::epoch_req *> (connection->requests.front().release()))));
            response->send_next();
        }
        void epoch_bulk_pull (germ::epoch_bulk_pull const &) override
        {
            auto response (std::make_shared<germ::tcp_epoch_bulk_pull_server> (connection, std::unique_ptr<germ::epoch_bulk_pull> (static_cast<germ::epoch_bulk_pull *> (connection->requests.front().release()))));
            response->send_next();
        }
        void epoch_bulk_push (germ::epoch_bulk_push const &) override
        {
            auto response (std::make_shared<germ::tcp_epoch_bulk_push_server> (connection));
            response->receive();
        }
        void transaction (germ::transaction_message const &) override
        {
            assert(false);
        }

        std::shared_ptr<germ::tcp_bootstrap_server> connection;
    };
}

void germ::tcp_bootstrap_server::run_next ()
{
    assert (!requests.empty ());
    request_response_visitor visitor (shared_from_this ());
    requests.front ()->visit (visitor);
}
