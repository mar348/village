//
// Created by daoful on 18-7-26.
//

#include <src/node/node.hpp>
#include <src/node/bootstrap/bootstrap_client.h>
#include <src/node/bootstrap/bootstrap_attempt.h>
#include <src/node/bootstrap/socket.h>



germ::tcp_bootstrap_client::tcp_bootstrap_client (std::shared_ptr<germ::node> node_a, std::shared_ptr<germ::tcp_bootstrap_attempt> attempt_a, germ::tcp_endpoint const & endpoint_a) :
        node (node_a),
        attempt (attempt_a),
        socket (std::make_shared<germ::tcp_socket> (node_a)),
        receive_buffer (std::make_shared<std::vector<uint8_t>> ()),
        endpoint (endpoint_a),
        start_time (std::chrono::steady_clock::now ()),
        block_count (0),
        pending_stop (false),
        hard_stop (false)
{
    ++attempt->connections;
    receive_buffer->resize (512);
}

germ::tcp_bootstrap_client::~tcp_bootstrap_client ()
{
    --attempt->connections;
}

double germ::tcp_bootstrap_client::block_rate () const
{
    auto elapsed = elapsed_seconds ();
    return elapsed > 0.0 ? (double)block_count.load () / elapsed : 0.0;
}

double germ::tcp_bootstrap_client::elapsed_seconds () const
{
    return std::chrono::duration_cast<std::chrono::duration<double>> (std::chrono::steady_clock::now () - start_time).count ();
}

void germ::tcp_bootstrap_client::stop (bool force)
{
    pending_stop = true;
    if (force)
    {
        hard_stop = true;
    }
}

void germ::tcp_bootstrap_client::run ()
{
    auto this_l (shared_from_this ());
    socket->async_connect (endpoint, [this_l](boost::system::error_code const & ec) {
        if (!ec)
        {
            if (this_l->node->config.logging.bulk_pull_logging ())
            {
                BOOST_LOG (this_l->node->log) << boost::str (boost::format ("Connection established to %1%") % this_l->endpoint);
            }
            this_l->attempt->pool_connection (this_l->shared_from_this ());
        }
        else
        {
            if (this_l->node->config.logging.network_logging ())
            {
                switch (ec.value ())
                {
                    default:
                        BOOST_LOG (this_l->node->log) << boost::str (boost::format ("Error initiating bootstrap connection to %1%: %2%") % this_l->endpoint % ec.message ());
                        break;
                    case boost::system::errc::connection_refused:
                    case boost::system::errc::operation_canceled:
                    case boost::system::errc::timed_out:
                    case 995: //Windows The I/O operation has been aborted because of either a thread exit or an application request
                    case 10061: //Windows No connection could be made because the target machine actively refused it
                        break;
                }
            }
        }
    });
}

std::shared_ptr<germ::tcp_bootstrap_client> germ::tcp_bootstrap_client::shared ()
{
    return shared_from_this ();
}