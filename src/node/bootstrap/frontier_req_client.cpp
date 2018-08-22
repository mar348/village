//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/frontier_req_client.h>
#include <src/node/bootstrap/bootstrap_attempt.h>
#include <src/node/bootstrap/bootstrap_client.h>
#include <src/node/bootstrap/socket.h>

constexpr unsigned epoch_bulk_push_cost_limit = 200;
constexpr double bootstrap_connection_warmup_time_sec = 5.0;
constexpr double bootstrap_minimum_frontier_blocks_per_sec = 1000.0;

germ::tcp_frontier_req_client::tcp_frontier_req_client (std::shared_ptr<germ::tcp_bootstrap_client> connection_a) :
        connection (connection_a),
        current (0),
        count (0),
        bulk_push_cost (0)
{
    germ::transaction transaction (connection->node->store.environment, nullptr, false);
    next (transaction);
}

germ::tcp_frontier_req_client::~tcp_frontier_req_client ()
{
}

void germ::tcp_frontier_req_client::run ()
{
    std::unique_ptr<germ::frontier_req> request (new germ::frontier_req);
    request->start.clear ();
    request->age = std::numeric_limits<decltype (request->age)>::max ();
    request->count = std::numeric_limits<decltype (request->age)>::max ();
    auto send_buffer (std::make_shared<std::vector<uint8_t>> ());
    {
        germ::vectorstream stream (*send_buffer);
        request->serialize (stream);
    }
    auto this_l (shared_from_this ());
    connection->socket->async_write (send_buffer, [this_l, send_buffer](boost::system::error_code const & ec, size_t size_a) {
        if (!ec)
        {
            this_l->receive_frontier ();
        }
        else
        {
            if (!this_l->connection->node->config.logging.network_logging ())
                return ;

            BOOST_LOG (this_l->connection->node->log) << boost::str (boost::format ("Error while sending bootstrap request %1%") % ec.message ());
        }
    });
}

void germ::tcp_frontier_req_client::receive_frontier ()
{
    auto this_l (shared_from_this ());
    size_t size_l (sizeof (germ::uint256_union) + sizeof (germ::uint256_union));
    connection->socket->async_read (connection->receive_buffer, size_l, [this_l, size_l](boost::system::error_code const & ec, size_t size_a) {
        // An issue with asio is that sometimes, instead of reporting a bad file descriptor during disconnect,
        // we simply get a size of 0.
        if (size_a == size_l)
        {
            this_l->received_frontier (ec, size_a);
        }
        else
        {
            std::cout << "tcp_frontier_req_client::receive_frontier: " << ec.message() << std::endl;
            if (!this_l->connection->node->config.logging.network_message_logging ())
                return ;

            BOOST_LOG (this_l->connection->node->log) << boost::str (boost::format ("Invalid size: expected %1%, got %2%") % size_l % size_a);
        }
    });
}

void germ::tcp_frontier_req_client::unsynced (MDB_txn * transaction_a, germ::block_hash const & head, germ::block_hash const & end)
{
    if (bulk_push_cost < epoch_bulk_push_cost_limit)
    {
        connection->attempt->add_bulk_push_target (head, end);
        if (end.is_zero ())
        {
            bulk_push_cost += 2;
        }
        else
        {
            bulk_push_cost += 1;
        }
    }
}

void germ::tcp_frontier_req_client::received_frontier (boost::system::error_code const & ec, size_t size_a)
{
    if (!ec)
    {
        assert (size_a == sizeof (germ::uint256_union) + sizeof (germ::uint256_union));
        germ::account account;
        germ::bufferstream account_stream (connection->receive_buffer->data (), sizeof (germ::uint256_union));
        auto error1 (germ::read (account_stream, account));
        assert (!error1);
        germ::block_hash latest;
        germ::bufferstream latest_stream (connection->receive_buffer->data () + sizeof (germ::uint256_union), sizeof (germ::uint256_union));
        auto error2 (germ::read (latest_stream, latest));
        assert (!error2);
        if (count == 0)
        {
            start_time = std::chrono::steady_clock::now ();
        }
        ++count;
        std::chrono::duration<double> time_span = std::chrono::duration_cast<std::chrono::duration<double>> (std::chrono::steady_clock::now () - start_time);
        double elapsed_sec = time_span.count ();
        double blocks_per_sec = (double)count / elapsed_sec;
        if (elapsed_sec > bootstrap_connection_warmup_time_sec && blocks_per_sec < bootstrap_minimum_frontier_blocks_per_sec)
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Aborting frontier req because it was too slow"));
            promise.set_value (false);
            return;
        }
        if (connection->attempt->should_log ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Received %1% frontiers from %2%") % std::to_string (count) % connection->socket->remote_endpoint ());
        }
        if (!account.is_zero ())
        {
            while (!current.is_zero () && current < account)
            {
                // We know about an account they don't.
                germ::transaction transaction (connection->node->store.environment, nullptr, true);
                unsynced (transaction, info.head, 0);
                next (transaction);
            }
            if (!current.is_zero ())
            {
                if (account == current)
                {
                    germ::transaction transaction (connection->node->store.environment, nullptr, true);
                    if (latest == info.head)
                    {
                        // In sync
                    }
                    else
                    {
                        if (connection->node->store.block_exists (transaction, latest))
                        {
                            // We know about a block they don't.
                            unsynced (transaction, info.head, latest);
                        }
                        else
                        {
                            connection->attempt->add_pull (germ::pull_info (account, latest, info.head));
                            // Either we're behind or there's a fork we differ on
                            // Either way, bulk pushing will probably not be effective
                            bulk_push_cost += 5;
                        }
                    }
                    next (transaction);
                }
                else
                {
                    assert (account < current);
                    connection->attempt->add_pull (germ::pull_info (account, latest, germ::block_hash (0)));
                }
            }
            else
            {
                connection->attempt->add_pull (germ::pull_info (account, latest, germ::block_hash (0)));
            }
            receive_frontier ();
        }
        else
        {
            {
                germ::transaction transaction (connection->node->store.environment, nullptr, true);
                while (!current.is_zero ())
                {
                    // We know about an account they don't.
                    unsynced (transaction, info.head, 0);
                    next (transaction);
                }
            }
            if (connection->node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (connection->node->log) << "Bulk push cost: " << bulk_push_cost;
            }
            {
                try
                {
                    promise.set_value (true);
                }
                catch (std::future_error &)
                {
                }
                connection->attempt->pool_connection (connection);
            }
        }
    }
    else
    {
        if (connection->node->config.logging.network_logging ())
        {
            BOOST_LOG (connection->node->log) << boost::str (boost::format ("Error while receiving frontier %1%") % ec.message ());
        }
    }
}

void germ::tcp_frontier_req_client::next (MDB_txn * transaction_a)
{
    auto iterator (connection->node->store.latest_begin (transaction_a, germ::uint256_union (current.number () + 1)));
    if (iterator != connection->node->store.latest_end ())
    {
        current = germ::account (iterator->first.uint256 ());
        info = germ::account_info (iterator->second);
    }
    else
    {
        current.clear ();
    }
}
