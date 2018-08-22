//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bootstrap_initiator.h>
#include <src/node/bootstrap/bootstrap_attempt.h>


germ::tcp_bootstrap_initiator::tcp_bootstrap_initiator (germ::node & node_a) :
        node (node_a),
        stopped (false),
        thread ([this]() { run_bootstrap (); })
{
}

germ::tcp_bootstrap_initiator::~tcp_bootstrap_initiator ()
{
    stop ();
    thread.join ();
}

void germ::tcp_bootstrap_initiator::bootstrap ()
{
    std::unique_lock<std::mutex> lock (mutex);
    if (!stopped && attempt == nullptr)
    {
        node.stats.inc (germ::stat::type::bootstrap, germ::stat::detail::initiate, germ::stat::dir::out);
        attempt = std::make_shared<germ::tcp_bootstrap_attempt> (node.shared ());
        condition.notify_all ();
    }
}

void germ::tcp_bootstrap_initiator::bootstrap (germ::endpoint const & endpoint_a, bool add_to_peers)
{
    if (add_to_peers)
    {
        node.peers.insert (germ::map_endpoint_to_v6 (endpoint_a), germ::protocol_version);
    }
    std::unique_lock<std::mutex> lock (mutex);
    if (!stopped)
    {
        while (attempt != nullptr)
        {
            attempt->stop ();
            condition.wait (lock);
        }
        node.stats.inc (germ::stat::type::bootstrap, germ::stat::detail::initiate, germ::stat::dir::out);
        attempt = std::make_shared<germ::tcp_bootstrap_attempt> (node.shared ());
        attempt->add_connection (endpoint_a);
        condition.notify_all ();
    }
}

void germ::tcp_bootstrap_initiator::run_bootstrap ()
{
    std::unique_lock<std::mutex> lock (mutex);
    while (!stopped)
    {
        if (attempt != nullptr)
        {
            lock.unlock ();
            attempt->run ();
            lock.lock ();
            attempt = nullptr;
            condition.notify_all ();
        }
        else
        {
            condition.wait (lock);
        }
    }
}

void germ::tcp_bootstrap_initiator::add_observer (std::function<void(bool)> const & observer_a)
{
    std::lock_guard<std::mutex> lock (mutex);
    observers.push_back (observer_a);
}

bool germ::tcp_bootstrap_initiator::in_progress ()
{
    return current_attempt () != nullptr;
}

std::shared_ptr<germ::tcp_bootstrap_attempt> germ::tcp_bootstrap_initiator::current_attempt ()
{
    std::lock_guard<std::mutex> lock (mutex);
    return attempt;
}

void germ::tcp_bootstrap_initiator::stop ()
{
    std::unique_lock<std::mutex> lock (mutex);
    stopped = true;
    if (attempt != nullptr)
    {
        attempt->stop ();
    }
    condition.notify_all ();
}

void germ::tcp_bootstrap_initiator::notify_listeners (bool in_progress_a)
{
    for (auto & i : observers)
    {
        i (in_progress_a);
    }
}
