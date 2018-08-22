//
// Created by daoful on 18-7-26.
//

#ifndef SRC_NETWORK_H
#define SRC_NETWORK_H

#include <src/node/node.hpp>

namespace germ
{

class udp_network
{
public:
    udp_network (germ::node &, uint16_t);
    void receive ();
    void stop ();
    void receive_action (boost::system::error_code const &, size_t);
    void rpc_action (boost::system::error_code const &, size_t);
    void republish_vote (std::shared_ptr<germ::vote>);
    void republish_block (MDB_txn *, std::shared_ptr<germ::tx>);
    void republish (germ::block_hash const &, std::shared_ptr<std::vector<uint8_t>>, germ::endpoint);
    void publish_broadcast (std::vector<germ::peer_information> &, std::unique_ptr<germ::tx>);
    void confirm_send (germ::confirm_ack const &, std::shared_ptr<std::vector<uint8_t>>, germ::endpoint const &);
    void merge_peers (std::array<germ::endpoint, 8> const &);
    void send_keepalive (germ::endpoint const &);
    void send_node_id_handshake (germ::endpoint const &, boost::optional<germ::uint256_union> const & query, boost::optional<germ::uint256_union> const & respond_to);
    void broadcast_confirm_req (std::shared_ptr<germ::tx>);
    void broadcast_confirm_req_base (std::shared_ptr<germ::tx>, std::shared_ptr<std::vector<germ::peer_information>>, unsigned);
    void send_confirm_req (germ::endpoint const &, std::shared_ptr<germ::tx>);
    void send_buffer (uint8_t const *, size_t, germ::endpoint const &, std::function<void(boost::system::error_code const &, size_t)>);
    germ::endpoint endpoint ();
    germ::endpoint remote;
    std::array<uint8_t, 512> buffer;
    boost::asio::ip::udp::socket socket;
    std::mutex socket_mutex;
    boost::asio::ip::udp::resolver resolver;
    germ::node & node;
    bool on;
    static uint16_t const node_port = germ::rai_network == germ::germ_networks::germ_live_network ? 7075 : 54000;
};

}


#endif //SRC_NETWORK_H
