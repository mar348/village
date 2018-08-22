//
// Created by daoful on 18-7-26.
//

#include <src/node/network/network.h>


germ::udp_network::udp_network (germ::node & node_a, uint16_t port) :
        socket (node_a.service, germ::endpoint (boost::asio::ip::address_v6::any (), port)),
        resolver (node_a.service),
        node (node_a),
        on (true)
{
}

void germ::udp_network::receive ()
{
    if (node.config.logging.network_packet_logging ())
    {
        BOOST_LOG (node.log) << "Receiving packet";
    }
    std::unique_lock<std::mutex> lock (socket_mutex);
    socket.async_receive_from (boost::asio::buffer (buffer.data (), buffer.size ()), remote, [this](boost::system::error_code const & error, size_t size_a) {
        receive_action (error, size_a);
    });
}

void germ::udp_network::stop ()
{
    on = false;
    socket.close ();
    resolver.cancel ();
}

void germ::udp_network::send_keepalive (germ::endpoint const & endpoint_a)
{
    assert (endpoint_a.address ().is_v6 ());
    germ::keepalive message;
    node.peers.random_fill (message.peers);
    std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
    {
        germ::vectorstream stream (*bytes);
        message.serialize (stream);
    }
    if (node.config.logging.network_keepalive_logging ())
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Keepalive req sent to %1%") % endpoint_a);
    }
    std::weak_ptr<germ::node> node_w (node.shared ());
    send_buffer (bytes->data (), bytes->size (), endpoint_a, [bytes, node_w, endpoint_a](boost::system::error_code const & ec, size_t) {
        if (auto node_l = node_w.lock ())
        {
            if (ec && node_l->config.logging.network_keepalive_logging ())
            {
                BOOST_LOG (node_l->log) << boost::str (boost::format ("Error sending keepalive to %1%: %2%") % endpoint_a % ec.message ());
            }
            else
            {
                node_l->stats.inc (germ::stat::type::message, germ::stat::detail::keepalive, germ::stat::dir::out);
            }
        }
    });
}


void germ::udp_network::send_node_id_handshake (germ::endpoint const & endpoint_a, boost::optional<germ::uint256_union> const & query, boost::optional<germ::uint256_union> const & respond_to)
{
    assert (endpoint_a.address ().is_v6 ());
    boost::optional<std::pair<germ::account, germ::signature>> response (boost::none);
    if (respond_to)
    {
        response = std::make_pair (node.node_id.pub, germ::sign_message (node.node_id.prv, node.node_id.pub, *respond_to));
        assert (!germ::validate_message (response->first, *respond_to, response->second));
    }
    germ::node_id_handshake message (query, response);
    std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
    {
        germ::vectorstream stream (*bytes);
        message.serialize (stream);
    }
    if (node.config.logging.network_node_id_handshake_logging ())
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Node ID handshake sent with node ID %1% to %2%: query %3%, respond_to %4% (signature %5%)") % node.node_id.pub.to_account () % endpoint_a % (query ? query->to_string () : std::string ("[none]")) % (respond_to ? respond_to->to_string () : std::string ("[none]")) % (response ? response->second.to_string () : std::string ("[none]")));
    }
    node.stats.inc (germ::stat::type::message, germ::stat::detail::node_id_handshake, germ::stat::dir::out);
    std::weak_ptr<germ::node> node_w (node.shared ());
    send_buffer (bytes->data (), bytes->size (), endpoint_a, [bytes, node_w, endpoint_a](boost::system::error_code const & ec, size_t) {
        if (auto node_l = node_w.lock ())
        {
            if (ec && node_l->config.logging.network_node_id_handshake_logging ())
            {
                BOOST_LOG (node_l->log) << boost::str (boost::format ("Error sending node ID handshake to %1% %2%") % endpoint_a % ec.message ());
            }
        }
    });
}

void germ::udp_network::republish (germ::block_hash const & hash_a, std::shared_ptr<std::vector<uint8_t>> buffer_a, germ::endpoint endpoint_a)
{
    if (node.config.logging.network_publish_logging ())
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Publishing %1% to %2%") % hash_a.to_string () % endpoint_a);
    }
    std::weak_ptr<germ::node> node_w (node.shared ());
    send_buffer (buffer_a->data (), buffer_a->size (), endpoint_a, [buffer_a, node_w, endpoint_a](boost::system::error_code const & ec, size_t size) {
        if (auto node_l = node_w.lock ())
        {
            if (ec && node_l->config.logging.network_logging ())
            {
                BOOST_LOG (node_l->log) << boost::str (boost::format ("Error sending publish to %1%: %2%") % endpoint_a % ec.message ());
            }
            else
            {
                node_l->stats.inc (germ::stat::type::message, germ::stat::detail::publish, germ::stat::dir::out);
            }
        }
    });
}

template <typename T>
bool confirm_block (MDB_txn * transaction_a, germ::node & node_a, T & list_a, std::shared_ptr<germ::tx> block_a)
{
    bool result (false);
    if (node_a.config.enable_voting)
    {
        node_a.wallets.foreach_representative (transaction_a, [&result, &block_a, &list_a, &node_a, &transaction_a](germ::public_key const & pub_a, germ::raw_key const & prv_a) {
            result = true;
            auto vote (node_a.store.vote_generate (transaction_a, pub_a, prv_a, block_a));
            germ::confirm_ack confirm (vote);
            std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
            {
                germ::vectorstream stream (*bytes);
                confirm.serialize (stream);
            }
            for (auto j (list_a.begin ()), m (list_a.end ()); j != m; ++j)
            {
                node_a.network.confirm_send (confirm, bytes, *j);
            }
        });
    }
    return result;
}

template <>
bool confirm_block (MDB_txn * transaction_a, germ::node & node_a, germ::endpoint & peer_a, std::shared_ptr<germ::tx> block_a)
{
    std::array<germ::endpoint, 1> endpoints;
    endpoints[0] = peer_a;
    auto result (confirm_block (transaction_a, node_a, endpoints, std::move (block_a)));
    return result;
}

void germ::udp_network::republish_block (MDB_txn * transaction, std::shared_ptr<germ::tx> block)
{
    auto hash (block->hash ());
    auto list (node.peers.list_fanout ());
    // If we're a representative, broadcast a signed confirm, otherwise an unsigned publish
    if (!confirm_block (transaction, node, list, block))
    {
        germ::publish message (block);
        std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
        {
            germ::vectorstream stream (*bytes);
            message.serialize (stream);
        }
        auto hash (block->hash ());
        for (auto i (list.begin ()), n (list.end ()); i != n; ++i)
        {
            republish (hash, bytes, *i);
        }
        if (node.config.logging.network_logging ())
        {
            BOOST_LOG (node.log) << boost::str (boost::format ("Block %1% was republished to peers") % hash.to_string ());
        }
    }
    else
    {
        if (node.config.logging.network_logging ())
        {
            BOOST_LOG (node.log) << boost::str (boost::format ("Block %1% was confirmed to peers") % hash.to_string ());
        }
    }
}

// In order to rate limit network traffic we republish:
// 1) Only if they are a non-replay vote of a block that's actively settling. Settling blocks are limited by block PoW
// 2) The rep has a weight > Y to prevent creating a lot of small-weight accounts to send out votes
// 3) Only if a vote for this block from this representative hasn't been received in the previous X second.
//    This prevents rapid publishing of votes with increasing sequence numbers.
//
// These rules are implemented by the caller, not this function.
void germ::udp_network::republish_vote (std::shared_ptr<germ::vote> vote_a)
{
    germ::confirm_ack confirm (vote_a);
    std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
    {
        germ::vectorstream stream (*bytes);
        confirm.serialize (stream);
    }
    auto list (node.peers.list_fanout ());
    for (auto j (list.begin ()), m (list.end ()); j != m; ++j)
    {
        node.network.confirm_send (confirm, bytes, *j);
    }
}

void germ::udp_network::broadcast_confirm_req (std::shared_ptr<germ::tx> block_a)
{
    auto list (std::make_shared<std::vector<germ::peer_information>> (node.peers.representatives (std::numeric_limits<size_t>::max ())));
    if (list->empty () || node.online_reps.online_stake () == node.config.online_weight_minimum.number ())
    {
        // broadcast request to all peers
        list = std::make_shared<std::vector<germ::peer_information>> (node.peers.list_vector ());
    }
    broadcast_confirm_req_base (block_a, list, 0);
}

void germ::udp_network::broadcast_confirm_req_base (std::shared_ptr<germ::tx> block_a, std::shared_ptr<std::vector<germ::peer_information>> endpoints_a, unsigned delay_a)
{
    const size_t max_reps = 10;
    if (node.config.logging.network_logging ())
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Broadcasting confirm req for block %1% to %2% representatives") % block_a->hash ().to_string () % std::min (endpoints_a->size (), max_reps));
    }
    auto count (0);
    while (!endpoints_a->empty () && count < max_reps)
    {
        send_confirm_req (endpoints_a->back ().endpoint, block_a);
        endpoints_a->pop_back ();
        count++;
    }
    if (!endpoints_a->empty ())
    {
        std::weak_ptr<germ::node> node_w (node.shared ());
        node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (delay_a), [node_w, block_a, endpoints_a, delay_a]() {
            if (auto node_l = node_w.lock ())
            {
                node_l->network.broadcast_confirm_req_base (block_a, endpoints_a, delay_a + 50);
            }
        });
    }
}

void germ::udp_network::send_confirm_req (germ::endpoint const & endpoint_a, std::shared_ptr<germ::tx> block)
{
    germ::confirm_req message (block);
    std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
    {
        germ::vectorstream stream (*bytes);
        message.serialize (stream);
    }
    if (node.config.logging.network_message_logging ())
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Sending confirm req to %1%") % endpoint_a);
    }
    std::weak_ptr<germ::node> node_w (node.shared ());
    node.stats.inc (germ::stat::type::message, germ::stat::detail::confirm_req, germ::stat::dir::out);
    send_buffer (bytes->data (), bytes->size (), endpoint_a, [bytes, node_w](boost::system::error_code const & ec, size_t size) {
        if (auto node_l = node_w.lock ())
        {
            if (ec && node_l->config.logging.network_logging ())
            {
                BOOST_LOG (node_l->log) << boost::str (boost::format ("Error sending confirm request: %1%") % ec.message ());
            }
        }
    });
}

void germ::udp_network::send_buffer (uint8_t const * data_a, size_t size_a, germ::endpoint const & endpoint_a, std::function<void(boost::system::error_code const &, size_t)> callback_a)
{
    std::unique_lock<std::mutex> lock (socket_mutex);
    if (node.config.logging.network_packet_logging ())
    {
        BOOST_LOG (node.log) << "Sending packet";
    }
    socket.async_send_to (boost::asio::buffer (data_a, size_a), endpoint_a, [this, callback_a](boost::system::error_code const & ec, size_t size_a) {
        callback_a (ec, size_a);
        this->node.stats.add (germ::stat::type::traffic, germ::stat::dir::out, size_a);
        if (this->node.config.logging.network_packet_logging ())
        {
            BOOST_LOG (this->node.log) << "Packet send complete";
        }
    });
}


germ::endpoint germ::udp_network::endpoint ()
{
    boost::system::error_code ec;
    auto port (socket.local_endpoint (ec).port ());
    if (ec)
    {
        BOOST_LOG (node.log) << "Unable to retrieve port: " << ec.message ();
    }
    return germ::endpoint (boost::asio::ip::address_v6::loopback (), port);
}

template <typename T>
void rep_query (germ::node & node_a, T const & peers_a)
{
    germ::transaction transaction (node_a.store.environment, nullptr, false);
    std::shared_ptr<germ::tx> block (node_a.store.block_random (transaction));
    auto hash (block->hash ());
    node_a.rep_crawler.add (hash);
    for (auto i (peers_a.begin ()), n (peers_a.end ()); i != n; ++i)
    {
        node_a.peers.rep_request (*i);
        node_a.network.send_confirm_req (*i, block);
    }
    std::weak_ptr<germ::node> node_w (node_a.shared ());
    node_a.alarm.add (std::chrono::steady_clock::now () + std::chrono::seconds (5), [node_w, hash]() {
        if (auto node_l = node_w.lock ())
        {
            node_l->rep_crawler.remove (hash);
        }
    });
}

template <>
void rep_query (germ::node & node_a, germ::endpoint const & peers_a)
{
    std::array<germ::endpoint, 1> peers;
    peers[0] = peers_a;
    rep_query (node_a, peers);
}

namespace
{
    class network_message_visitor : public germ::message_visitor
    {
    public:
        network_message_visitor (germ::node & node_a, germ::endpoint const & sender_a) :
                node (node_a),
                sender (sender_a)
        {
        }
        virtual ~network_message_visitor () = default;
        void keepalive (germ::keepalive const & message_a) override
        {
            if (node.config.logging.network_keepalive_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Received keepalive message from %1%") % sender);
            }
            node.stats.inc (germ::stat::type::message, germ::stat::detail::keepalive, germ::stat::dir::in);
            if (node.peers.contacted (sender, message_a.header.version))
            {
                auto endpoint_l (germ::map_endpoint_to_v6 (sender));
                auto cookie (node.peers.assign_syn_cookie (endpoint_l));
                if (cookie)
                {
                    node.network.send_node_id_handshake (endpoint_l, *cookie, boost::none);
                }
            }
            node.network.merge_peers (message_a.peers);
        }
        void publish (germ::publish const & message_a) override
        {
            if (node.config.logging.network_message_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Publish message from %1% for %2%") % sender % message_a.block->hash ().to_string ());
            }
            node.stats.inc (germ::stat::type::message, germ::stat::detail::publish, germ::stat::dir::in);
            node.peers.contacted (sender, message_a.header.version);
            node.process_active (message_a.block);
        }
        void confirm_req (germ::confirm_req const & message_a) override
        {
            if (node.config.logging.network_message_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Confirm_req message from %1% for %2%") % sender % message_a.block->hash ().to_string ());
            }
            node.stats.inc (germ::stat::type::message, germ::stat::detail::confirm_req, germ::stat::dir::in);
            node.peers.contacted (sender, message_a.header.version);
            node.process_active (message_a.block);
            germ::transaction transaction_a (node.store.environment, nullptr, false);
            auto successor (node.ledger.successor (transaction_a, message_a.block->root ()));
            if (successor != nullptr)
            {
                confirm_block (transaction_a, node, sender, std::move (successor));
            }
        }
        void confirm_ack (germ::confirm_ack const & message_a) override
        {
            if (node.config.logging.network_message_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Received confirm_ack message from %1% for %2% sequence %3%") % sender % message_a.vote->block->hash ().to_string () % std::to_string (message_a.vote->sequence));
            }
            node.stats.inc (germ::stat::type::message, germ::stat::detail::confirm_ack, germ::stat::dir::in);
            node.peers.contacted (sender, message_a.header.version);
            node.process_active (message_a.vote->block);
            node.vote_processor.vote (message_a.vote, sender);
        }
        void bulk_pull (germ::bulk_pull const &) override
        {
            assert (false);
        }
        void bulk_pull_blocks (germ::bulk_pull_blocks const &) override
        {
            assert (false);
        }
        void bulk_push (germ::bulk_push const &) override
        {
            assert (false);
        }
        void frontier_req (germ::frontier_req const &) override
        {
            assert (false);
        }
        void node_id_handshake (germ::node_id_handshake const & message_a) override
        {
            if (node.config.logging.network_node_id_handshake_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Received node_id_handshake message from %1% with query %2% and response account %3%") % sender % (message_a.query ? message_a.query->to_string () : std::string ("[none]")) % (message_a.response ? message_a.response->first.to_account () : std::string ("[none]")));
            }
            node.stats.inc (germ::stat::type::message, germ::stat::detail::node_id_handshake, germ::stat::dir::in);
            auto endpoint_l (germ::map_endpoint_to_v6 (sender));
            boost::optional<germ::uint256_union> out_query;
            boost::optional<germ::uint256_union> out_respond_to;
            if (message_a.query)
            {
                out_respond_to = message_a.query;
            }
            auto validated_response (false);
            if (message_a.response)
            {
                if (!node.peers.validate_syn_cookie (endpoint_l, message_a.response->first, message_a.response->second))
                {
                    validated_response = true;
                    if (message_a.response->first != node.node_id.pub)
                    {
                        node.peers.insert (endpoint_l, message_a.header.version);
                    }
                }
                else if (node.config.logging.network_node_id_handshake_logging ())
                {
                    BOOST_LOG (node.log) << boost::str (boost::format ("Failed to validate syn cookie signature %1% by %2%") % message_a.response->second.to_string () % message_a.response->first.to_account ());
                }
            }
            if (!validated_response && !node.peers.known_peer (endpoint_l))
            {
                out_query = node.peers.assign_syn_cookie (endpoint_l);
            }
            if (out_query || out_respond_to)
            {
                node.network.send_node_id_handshake (sender, out_query, out_respond_to);
            }
        }
        void epoch_req (germ::epoch_req const & tx_r) override
        {
            assert(false);
        }
        void epoch_bulk_pull (germ::epoch_bulk_pull const &) override
        {
            assert(false);
        }
        void epoch_bulk_push (germ::epoch_bulk_push const &) override
        {
            assert(false);
        }
        void transaction (germ::transaction_message const &) override
        {
            assert(false);
        }
        
        germ::node & node;
        germ::endpoint sender;
    };
}

void germ::udp_network::receive_action (boost::system::error_code const & error, size_t size_a)
{
    if (!error && on)
    {
        if (!germ::reserved_address (remote, false) && remote != endpoint ())
        {
            network_message_visitor visitor (node, remote);
            germ::message_parser parser (visitor, node.work);
            parser.deserialize_buffer (buffer.data (), size_a);
            if (parser.status != germ::message_parser::parse_status::success)
            {
                node.stats.inc (germ::stat::type::error);

               if (parser.status == germ::message_parser::parse_status::invalid_message_type)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid message type in message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_header)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid header in message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_keepalive_message)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid keepalive message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_publish_message)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid publish message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_confirm_req_message)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid confirm_req message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_confirm_ack_message)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid confirm_ack message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_node_id_handshake_message)
                {
                    if (node.config.logging.network_logging ())
                    {
                        BOOST_LOG (node.log) << "Invalid node_id_handshake message";
                    }
                }
                else if (parser.status == germ::message_parser::parse_status::invalid_epoch_req_message)
                {
                    if (node.config.logging.network_logging())
                    {
                        BOOST_LOG(node.log) << "Invalid transaction message";
                    }
                }
                else
                {
                    BOOST_LOG (node.log) << "Could not deserialize buffer";
                }
            }
            else
            {
                node.stats.add (germ::stat::type::traffic, germ::stat::dir::in, size_a);
            }
        }
        else
        {
            if (node.config.logging.network_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Reserved sender %1%") % remote.address ().to_string ());
            }

            node.stats.inc_detail_only (germ::stat::type::error, germ::stat::detail::bad_sender);
        }
        receive ();
    }
    else
    {
        if (error)
        {
            if (node.config.logging.network_logging ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("UDP Receive error: %1%") % error.message ());
            }
        }
        if (on)
        {
            node.alarm.add (std::chrono::steady_clock::now () + std::chrono::seconds (5), [this]() { receive (); });
        }
    }
}

// Send keepalives to all the peers we've been notified of
void germ::udp_network::merge_peers (std::array<germ::endpoint, 8> const & peers_a)
{
    for (auto i (peers_a.begin ()), j (peers_a.end ()); i != j; ++i)
    {
        if (!node.peers.reachout (*i))
        {
            send_keepalive (*i);
        }
    }
}


void germ::udp_network::confirm_send (germ::confirm_ack const & confirm_a, std::shared_ptr<std::vector<uint8_t>> bytes_a, germ::endpoint const & endpoint_a)
{
    if (node.config.logging.network_publish_logging ())
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Sending confirm_ack for block %1% to %2% sequence %3%") % confirm_a.vote->block->hash ().to_string () % endpoint_a % std::to_string (confirm_a.vote->sequence));
    }
    std::weak_ptr<germ::node> node_w (node.shared ());
    node.network.send_buffer (bytes_a->data (), bytes_a->size (), endpoint_a, [bytes_a, node_w, endpoint_a](boost::system::error_code const & ec, size_t size_a) {
        if (auto node_l = node_w.lock ())
        {
            if (ec && node_l->config.logging.network_logging ())
            {
                BOOST_LOG (node_l->log) << boost::str (boost::format ("Error broadcasting confirm_ack to %1%: %2%") % endpoint_a % ec.message ());
            }
            else
            {
                node_l->stats.inc (germ::stat::type::message, germ::stat::detail::confirm_ack, germ::stat::dir::out);
            }
        }
    });
}
