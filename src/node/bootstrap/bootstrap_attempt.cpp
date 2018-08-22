//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bootstrap_attempt.h>
#include <src/node/bootstrap/bootstrap_client.h>
#include <src/node/bootstrap/frontier_req_client.h>
#include <src/node/bootstrap/bulk_push_client.h>
#include <src/node/bootstrap/bulk_pull_client.h>
#include <src/node/bootstrap/socket.h>


constexpr double bootstrap_connection_scale_target_blocks = 50000.0;
constexpr double bootstrap_connection_warmup_time_sec = 5.0;
constexpr double bootstrap_minimum_blocks_per_sec = 10.0;
constexpr double bootstrap_minimum_frontier_blocks_per_sec = 1000.0;
constexpr unsigned bootstrap_frontier_retry_limit = 16;
constexpr double bootstrap_minimum_termination_time_sec = 30.0;
constexpr unsigned bootstrap_max_new_connections = 10;
constexpr unsigned epoch_bulk_push_cost_limit = 200;

germ::tcp_bootstrap_attempt::tcp_bootstrap_attempt (std::shared_ptr<germ::node> node_a) :
        next_log (std::chrono::steady_clock::now ()),
        connections (0),
        pulling (0),
        node (node_a),
        account_count (0),
        total_blocks (0),
        stopped (false)
{
    BOOST_LOG (node->log) << "Starting bootstrap attempt";
    node->bootstrap_initiator.notify_listeners (true);
}

germ::tcp_bootstrap_attempt::~tcp_bootstrap_attempt ()
{
    BOOST_LOG (node->log) << "Exiting bootstrap attempt";
    node->bootstrap_initiator.notify_listeners (false);
}

bool germ::tcp_bootstrap_attempt::should_log ()
{
    std::lock_guard<std::mutex> lock (mutex);
    auto result (false);
    auto now (std::chrono::steady_clock::now ());
    if (next_log < now)
    {
        result = true;
        next_log = now + std::chrono::seconds (15);
    }
    return result;
}

bool germ::tcp_bootstrap_attempt::request_frontier (std::unique_lock<std::mutex> & lock_a)
{
    auto result (true);
    auto connection_l (connection (lock_a));
    connection_frontier_request = connection_l;
    if (!connection_l)
        return result;

    std::future<bool> future;
    {
        auto client (std::make_shared<germ::tcp_frontier_req_client> (connection_l));
        client->run ();
        frontiers = client;
        future = client->promise.get_future ();
    }
    lock_a.unlock ();
    result = consume_future (future);
    lock_a.lock ();
    if (!result)
    {
        pulls.clear ();
    }
    if (node->config.logging.network_logging ())
    {
        if (result)
        {
            BOOST_LOG (node->log) << boost::str (boost::format ("Completed frontier request, %1% out of sync accounts according to %2%") % pulls.size () % connection_l->endpoint);
        }
        else
        {
            BOOST_LOG (node->log) << "frontier_req failed, reattempting";
        }
    }
    return !result;
}

void germ::tcp_bootstrap_attempt::request_pull (std::unique_lock<std::mutex> & lock_a)
{
    auto connection_l (connection (lock_a));
    if (connection_l)
    {
        auto pull (pulls.front ());
        pulls.pop_front ();
        // The tcp_bulk_pull_client destructor attempt to requeue_pull which can cause a deadlock if this is the last reference
        // Dispatch request in an external thread in case it needs to be destroyed
        node->background ([connection_l, pull]() {
            auto client (std::make_shared<germ::tcp_bulk_pull_client> (connection_l, pull));
            client->request ();
        });
    }
}

void germ::tcp_bootstrap_attempt::request_push (std::unique_lock<std::mutex> & lock_a)
{
    bool error (false);
    if (auto connection_shared = connection_frontier_request.lock ())
    {
        auto client (std::make_shared<germ::tcp_bulk_push_client> (connection_shared));
        client->start ();
        push = client;
        auto future (client->promise.get_future ());
        lock_a.unlock ();
        error = consume_future (future);
        lock_a.lock ();
    }
    if (node->config.logging.network_logging ())
    {
        BOOST_LOG (node->log) << "Exiting bulk push client";
        if (!error)
        {
            BOOST_LOG (node->log) << "Bulk push client failed";
        }
    }
}

bool germ::tcp_bootstrap_attempt::still_pulling ()
{
    assert (!mutex.try_lock ());
    auto running (!stopped);
    auto more_pulls (!pulls.empty ());
    auto still_pulling (pulling > 0);
    return running && (more_pulls || still_pulling);
}

void germ::tcp_bootstrap_attempt::run ()
{
    populate_connections ();
    std::unique_lock<std::mutex> lock (mutex);
    auto frontier_failure (true);
    while (!stopped && frontier_failure)
    {
        frontier_failure = request_frontier (lock);
    }
    // Shuffle pulls.
    for (int i = pulls.size () - 1; i > 0; i--)
    {
        auto k = germ::random_pool.GenerateWord32 (0, i);
        std::swap (pulls[i], pulls[k]);
    }
    while (still_pulling ())
    {
        while (still_pulling ())
        {
            if (!pulls.empty ())
            {
                if (!node->block_processor.full ())
                {
                    request_pull (lock);
                }
                else
                {
                    condition.wait_for (lock, std::chrono::seconds (15));
                }
            }
            else
            {
                condition.wait (lock);
            }
        }
        // Flushing may resolve forks which can add more pulls
        BOOST_LOG (node->log) << "Flushing unchecked blocks";
        lock.unlock ();
        node->block_processor.flush ();
        lock.lock ();
        BOOST_LOG (node->log) << "Finished flushing unchecked blocks";
    }
    if (!stopped)
    {
        BOOST_LOG (node->log) << "Completed pulls";
    }
    request_push (lock);
    stopped = true;
    condition.notify_all ();
    idle.clear ();
}

std::shared_ptr<germ::tcp_bootstrap_client> germ::tcp_bootstrap_attempt::connection (std::unique_lock<std::mutex> & lock_a)
{
    while (!stopped && idle.empty ())
    {
        condition.wait (lock_a);
    }
    std::shared_ptr<germ::tcp_bootstrap_client> result;
    if (!idle.empty ())
    {
        result = idle.back ();
        idle.pop_back ();
    }
    return result;
}

bool germ::tcp_bootstrap_attempt::consume_future (std::future<bool> & future_a)
{
    bool result;
    try
    {
        result = future_a.get ();
    }
    catch (std::future_error &e)
    {
        result = false;
    }
    return result;
}

struct block_rate_cmp
{
    bool operator() (const std::shared_ptr<germ::tcp_bootstrap_client> & lhs, const std::shared_ptr<germ::tcp_bootstrap_client> & rhs) const
    {
        return lhs->block_rate () > rhs->block_rate ();
    }
};

unsigned germ::tcp_bootstrap_attempt::target_connections (size_t pulls_remaining)
{
    if (node->config.bootstrap_connections >= node->config.bootstrap_connections_max)
    {
        return std::max (1U, node->config.bootstrap_connections_max);
    }

    // Only scale up to bootstrap_connections_max for large pulls.
    double step = std::min (1.0, std::max (0.0, (double)pulls_remaining / bootstrap_connection_scale_target_blocks));
    double target = (double)node->config.bootstrap_connections + (double)(node->config.bootstrap_connections_max - node->config.bootstrap_connections) * step;
    return std::max (1U, (unsigned)(target + 0.5f));
}

void germ::tcp_bootstrap_attempt::populate_connections ()
{
    double rate_sum = 0.0;
    size_t num_pulls = 0;
    std::priority_queue<std::shared_ptr<germ::tcp_bootstrap_client>, std::vector<std::shared_ptr<germ::tcp_bootstrap_client>>, block_rate_cmp> sorted_connections;
    {
        std::unique_lock<std::mutex> lock (mutex);
        num_pulls = pulls.size ();
        for (auto & c : clients)
        {
            if (auto client = c.lock ())
            {
                double elapsed_sec = client->elapsed_seconds ();
                auto blocks_per_sec = client->block_rate ();
                rate_sum += blocks_per_sec;
                if (client->elapsed_seconds () > bootstrap_connection_warmup_time_sec && client->block_count > 0)
                {
                    sorted_connections.push (client);
                }
                // Force-stop the slowest peers, since they can take the whole bootstrap hostage by dribbling out blocks on the last remaining pull.
                // This is ~1.5kilobits/sec.
                if (elapsed_sec > bootstrap_minimum_termination_time_sec && blocks_per_sec < bootstrap_minimum_blocks_per_sec)
                {
                    if (node->config.logging.bulk_pull_logging ())
                    {
                        BOOST_LOG (node->log) << boost::str (boost::format ("Stopping slow peer %1% (elapsed sec %2%s > %3%s and %4% blocks per second < %5%)") % client->endpoint.address ().to_string () % elapsed_sec % bootstrap_minimum_termination_time_sec % blocks_per_sec % bootstrap_minimum_blocks_per_sec);
                    }

                    client->stop (true);
                }
            }
        }
    }

    auto target = target_connections (num_pulls);

    // We only want to drop slow peers when more than 2/3 are active. 2/3 because 1/2 is too aggressive, and 100% rarely happens.
    // Probably needs more tuning.
    if (sorted_connections.size () >= (target * 2) / 3 && target >= 4)
    {
        // 4 -> 1, 8 -> 2, 16 -> 4, arbitrary, but seems to work well.
        auto drop = (int)roundf (sqrtf ((float)target - 2.0f));

        if (node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (node->log) << boost::str (boost::format ("Dropping %1% bulk pull peers, target connections %2%") % drop % target);
        }

        for (int i = 0; i < drop; i++)
        {
            auto client = sorted_connections.top ();

            if (node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (node->log) << boost::str (boost::format ("Dropping peer with block rate %1%, block count %2% (%3%) ") % client->block_rate () % client->block_count % client->endpoint.address ().to_string ());
            }

            client->stop (false);
            sorted_connections.pop ();
        }
    }

    if (node->config.logging.bulk_pull_logging ())
    {
        std::unique_lock<std::mutex> lock (mutex);
        BOOST_LOG (node->log) << boost::str (boost::format ("Bulk pull connections: %1%, rate: %2% blocks/sec, remaining account pulls: %3%, total blocks: %4%") % connections.load () % (int)rate_sum % pulls.size () % (int)total_blocks.load ());
    }

    if (connections < target)
    {
        auto delta = std::min ((target - connections) * 2, bootstrap_max_new_connections);
        // TODO - tune this better
        // Not many peers respond, need to try to make more connections than we need.
        for (int i = 0; i < delta; i++)
        {
            auto peer (node->peers.bootstrap_peer ());
            if (peer != germ::endpoint (boost::asio::ip::address_v6::any (), 0))
            {
                auto client (std::make_shared<germ::tcp_bootstrap_client> (node, shared_from_this (), germ::tcp_endpoint (peer.address (), peer.port ())));
                client->run ();
                std::lock_guard<std::mutex> lock (mutex);
                clients.push_back (client);
            }
            else if (connections == 0)
            {
                BOOST_LOG (node->log) << boost::str (boost::format ("Bootstrap stopped because there are no peers"));
                stopped = true;
                condition.notify_all ();
            }
        }
    }
    if (!stopped)
    {
        std::weak_ptr<germ::tcp_bootstrap_attempt> this_w (shared_from_this ());
        node->alarm.add (std::chrono::steady_clock::now () + std::chrono::seconds (1), [this_w]() {
            if (auto this_l = this_w.lock ())
            {
                this_l->populate_connections ();
            }
        });
    }
}

void germ::tcp_bootstrap_attempt::add_connection (germ::endpoint const & endpoint_a)
{
    auto client (std::make_shared<germ::tcp_bootstrap_client> (node, shared_from_this (), germ::tcp_endpoint (endpoint_a.address (), endpoint_a.port ())));
    client->run ();
}

void germ::tcp_bootstrap_attempt::pool_connection (std::shared_ptr<germ::tcp_bootstrap_client> client_a)
{
    std::lock_guard<std::mutex> lock (mutex);
    idle.push_front (client_a);
    condition.notify_all ();
}

void germ::tcp_bootstrap_attempt::stop ()
{
    std::lock_guard<std::mutex> lock (mutex);
    stopped = true;
    condition.notify_all ();
    for (auto i : clients)
    {
        if (auto client = i.lock ())
        {
            client->socket->close ();
        }
    }
    if (auto i = frontiers.lock ())
    {
        try
        {
            i->promise.set_value (false);
        }
        catch (std::future_error &)
        {
        }
    }
    if (auto i = push.lock ())
    {
        try
        {
            i->promise.set_value (false);
        }
        catch (std::future_error &)
        {
        }
    }
}

void germ::tcp_bootstrap_attempt::add_pull (germ::pull_info const & pull)
{
    std::lock_guard<std::mutex> lock (mutex);
    pulls.push_back (pull);
    condition.notify_all ();
}

void germ::tcp_bootstrap_attempt::requeue_pull (germ::pull_info const & pull_a)
{
    auto pull (pull_a);
    if (++pull.attempts < bootstrap_frontier_retry_limit)
    {
        std::lock_guard<std::mutex> lock (mutex);
        pulls.push_front (pull);
        condition.notify_all ();
    }
    else if (pull.attempts == bootstrap_frontier_retry_limit)
    {
        pull.attempts++;
        std::lock_guard<std::mutex> lock (mutex);
        if (auto connection_shared = connection_frontier_request.lock ())
        {
            node->background ([connection_shared, pull]() {
                auto client (std::make_shared<germ::tcp_bulk_pull_client> (connection_shared, pull));
                client->request ();
            });
            if (node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (node->log) << boost::str (boost::format ("Requesting pull account %1% from frontier peer after %2% attempts") % pull.account.to_account () % pull.attempts);
            }
        }
    }
    else
    {
        if (node->config.logging.bulk_pull_logging ())
        {
            BOOST_LOG (node->log) << boost::str (boost::format ("Failed to pull account %1% down to %2% after %3% attempts") % pull.account.to_account () % pull.end.to_string () % pull.attempts);
        }
    }
}

void germ::tcp_bootstrap_attempt::add_bulk_push_target (germ::block_hash const & head, germ::block_hash const & end)
{
    std::lock_guard<std::mutex> lock (mutex);
    bulk_push_targets.push_back (std::make_pair (head, end));
}

