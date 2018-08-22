#pragma once

#include <src/common.hpp>
#include <src/lib/interface.h>

#include <boost/asio.hpp>

#include <bitset>

#include <xxhash/xxhash.h>

#include <src/lib/epoch.h>
#include <unordered_set>

namespace germ
{
using endpoint = boost::asio::ip::udp::endpoint;
bool parse_port (std::string const &, uint16_t &);
bool parse_address_port (std::string const &, boost::asio::ip::address &, uint16_t &);
using tcp_endpoint = boost::asio::ip::tcp::endpoint;
bool parse_endpoint (std::string const &, germ::endpoint &);
bool parse_tcp_endpoint (std::string const &, germ::tcp_endpoint &);
bool reserved_address (germ::endpoint const &, bool);
}
static uint64_t endpoint_hash_raw (germ::endpoint const & endpoint_a)
{
    assert (endpoint_a.address ().is_v6 ());
    germ::uint128_union address;
    address.bytes = endpoint_a.address ().to_v6 ().to_bytes ();
    XXH64_state_t hash;
    XXH64_reset (&hash, 0);
    XXH64_update (&hash, address.bytes.data (), address.bytes.size ());
    auto port (endpoint_a.port ());
    XXH64_update (&hash, &port, sizeof (port));
    auto result (XXH64_digest (&hash));
    return result;
}
static uint64_t ip_address_hash_raw (boost::asio::ip::address const & ip_a)
{
    assert (ip_a.is_v6 ());
    germ::uint128_union bytes;
    bytes.bytes = ip_a.to_v6 ().to_bytes ();
    XXH64_state_t hash;
    XXH64_reset (&hash, 0);
    XXH64_update (&hash, bytes.bytes.data (), bytes.bytes.size ());
    auto result (XXH64_digest (&hash));
    return result;
}

namespace std
{
template <size_t size>
struct endpoint_hash
{
};
template <>
struct endpoint_hash<8>
{
    size_t operator() (germ::endpoint const & endpoint_a) const
    {
        return endpoint_hash_raw (endpoint_a);
    }
};
template <>
struct endpoint_hash<4>
{
    size_t operator() (germ::endpoint const & endpoint_a) const
    {
        uint64_t big (endpoint_hash_raw (endpoint_a));
        uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
        return result;
    }
};
template <>
struct hash<germ::endpoint>
{
    size_t operator() (germ::endpoint const & endpoint_a) const
    {
        endpoint_hash<sizeof (size_t)> ehash;
        return ehash (endpoint_a);
    }
};
template <size_t size>
struct ip_address_hash
{
};
template <>
struct ip_address_hash<8>
{
    size_t operator() (boost::asio::ip::address const & ip_address_a) const
    {
        return ip_address_hash_raw (ip_address_a);
    }
};
template <>
struct ip_address_hash<4>
{
    size_t operator() (boost::asio::ip::address const & ip_address_a) const
    {
        uint64_t big (ip_address_hash_raw (ip_address_a));
        uint32_t result (static_cast<uint32_t> (big) ^ static_cast<uint32_t> (big >> 32));
        return result;
    }
};
template <>
struct hash<boost::asio::ip::address>
{
    size_t operator() (boost::asio::ip::address const & ip_a) const
    {
        ip_address_hash<sizeof (size_t)> ihash;
        return ihash (ip_a);
    }
};
}
namespace boost
{
template <>
struct hash<germ::endpoint>
{
    size_t operator() (germ::endpoint const & endpoint_a) const
    {
        std::hash<germ::endpoint> hash;
        return hash (endpoint_a);
    }
};
}

namespace germ
{
enum class message_type : uint8_t
{
    invalid,
    not_a_type,
    keepalive,
    publish,
    confirm_req,
    confirm_ack,
    bulk_pull,
    bulk_push,
    frontier_req,
    bulk_pull_blocks,
    node_id_handshake,
    epoch_req,
    epoch_bulk_pull,
    epoch_bulk_push,
    transaction
};
enum class bulk_pull_blocks_mode : uint8_t
{
    list_blocks,
    checksum_blocks
};
class message_visitor;
class message_header
{
public:
    message_header (germ::message_type, size_t body_size_a);
    message_header (bool &, germ::stream &);
    void serialize (germ::stream &);
    bool deserialize (germ::stream &);
    germ::block_type block_type () const;
    void block_type_set (germ::block_type);
    bool ipv4_only ();
    void ipv4_only_set (bool);
    static std::array<uint8_t, 2> constexpr magic_number = germ::rai_network == germ::germ_networks::germ_test_network ? std::array<uint8_t, 2>{ { 'R', 'A' } } : germ::rai_network == germ::germ_networks::germ_beta_network ? std::array<uint8_t, 2>{ { 'R', 'B' } } : std::array<uint8_t, 2>{ { 'R', 'C' } };
    uint8_t version;
    germ::message_type type;
    std::bitset<16> extensions;
    static size_t constexpr ipv4_only_position = 1;
    static size_t constexpr bootstrap_server_position = 2;
    static std::bitset<16> constexpr block_type_mask = std::bitset<16> (0x0f00);
    size_t body_size;
};
class message
{
public:
    message (germ::message_type, size_t);
    message (germ::message_header const &);
    virtual ~message () = default;
    virtual void serialize (germ::stream &) = 0;
    virtual bool deserialize (germ::stream &) = 0;
    virtual void visit (germ::message_visitor &) const = 0;
    germ::message_header header;
};
class work_pool;
class message_parser
{
public:
    enum class parse_status
    {
        success,
        invalid_header,
        invalid_message_type,
        invalid_keepalive_message,
        invalid_publish_message,
        invalid_confirm_req_message,
        invalid_confirm_ack_message,
        invalid_node_id_handshake_message,
        invalid_epoch_req_message,
        invalid_epoch_bulk_pull_message,
        invalid_epoch_bulk_push_message,
        invalid_transaction_message
    };
    message_parser (germ::message_visitor &, germ::work_pool &);
    void deserialize_buffer (uint8_t const *, size_t);
    void deserialize_keepalive (germ::stream &, germ::message_header const &);
    void deserialize_publish (germ::stream &, germ::message_header const &);
    void deserialize_confirm_req (germ::stream &, germ::message_header const &);
    void deserialize_confirm_ack (germ::stream &, germ::message_header const &);
    void deserialize_node_id_handshake (germ::stream &, germ::message_header const &);

    void deserialize_epoch_req(germ::stream & stream_r, germ::message_header const & header_r);
    void deserialize_epoch_bulk_pull(germ::stream & stream_r, germ::message_header const & header_r);
    void deserialize_epoch_bulk_push(germ::stream & stream_r, germ::message_header const & header_r);

    void deserialize_transaction(germ::stream & stream_r, germ::message_header const & header_r);

    bool at_end (germ::stream &);
    germ::message_visitor & visitor;
    germ::work_pool & pool;
    parse_status status;
};
class keepalive : public message
{
public:
    keepalive (bool &, germ::stream &, germ::message_header const &);
    keepalive ();
    void visit (germ::message_visitor &) const override;
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    bool operator== (germ::keepalive const &) const;
    std::array<germ::endpoint, 8> peers;
};
class publish : public message
{
public:
    publish (bool &, germ::stream &, germ::message_header const &);
    publish (std::shared_ptr<germ::tx>);
    void visit (germ::message_visitor &) const override;
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    bool operator== (germ::publish const &) const;
    std::shared_ptr<germ::tx> block;
};
class confirm_req : public message
{
public:
    confirm_req (bool &, germ::stream &, germ::message_header const &);
    confirm_req (std::shared_ptr<germ::tx>);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    bool operator== (germ::confirm_req const &) const;
    std::shared_ptr<germ::tx> block;
};
class confirm_ack : public message
{
public:
    confirm_ack (bool &, germ::stream &, germ::message_header const &);
    confirm_ack (std::shared_ptr<germ::vote>);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    bool operator== (germ::confirm_ack const &) const;
    std::shared_ptr<germ::vote> vote;
};
class frontier_req : public message
{
public:
    frontier_req ();
    frontier_req (bool &, germ::stream &, germ::message_header const &);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    bool operator== (germ::frontier_req const &) const;
    germ::account start;
    uint32_t age;
    uint32_t count;
};
class bulk_pull : public message
{
public:
    bulk_pull ();
    bulk_pull (bool &, germ::stream &, germ::message_header const &);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    germ::uint256_union start;
    germ::block_hash end;
};
class bulk_pull_blocks : public message
{
public:
    bulk_pull_blocks ();
    bulk_pull_blocks (bool &, germ::stream &, germ::message_header const &);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    germ::block_hash min_hash;
    germ::block_hash max_hash;
    bulk_pull_blocks_mode mode;
    uint32_t max_count;
};
class bulk_push : public message
{
public:
    bulk_push ();
    bulk_push (germ::message_header const &);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
};
class node_id_handshake : public message
{
public:
    node_id_handshake (bool &, germ::stream &, germ::message_header const &);
    node_id_handshake (boost::optional<germ::block_hash>, boost::optional<std::pair<germ::account, germ::signature>>);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    bool operator== (germ::node_id_handshake const &) const;
    boost::optional<germ::uint256_union> query;
    boost::optional<std::pair<germ::account, germ::signature>> response;
    static size_t constexpr query_flag = 0;
    static size_t constexpr response_flag = 1;
};

class epoch_req : public message
{
public:
    epoch_req();
    epoch_req(bool & error_r, germ::stream & stream_r, germ::message_header const & header_r);
    bool deserialize (germ::stream & stream_r) override ;
    void serialize (germ::stream & stream_r) override ;
    void visit (germ::message_visitor & visit_r) const override ;
    bool operator== (germ::epoch_req const & epoch) const;

    germ::epoch_hash start;
//    germ::epoch_hash end;
    uint32_t age;
    uint32_t count;
};

class epoch_bulk_pull : public message
{
public:
    epoch_bulk_pull ();
    epoch_bulk_pull (bool &, germ::stream &, germ::message_header const &);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    germ::epoch_hash start;
    germ::epoch_hash end;
};

class epoch_bulk_push : public message
{
public:
    epoch_bulk_push ();
    epoch_bulk_push (germ::message_header const &);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
};

class transaction_message : public message
{
public:
    transaction_message (bool &, germ::stream &, germ::message_header const &);
    transaction_message (std::shared_ptr<germ::tx>);
    bool deserialize (germ::stream &) override;
    void serialize (germ::stream &) override;
    void visit (germ::message_visitor &) const override;
    bool operator== (germ::transaction_message const &) const;
    std::shared_ptr<germ::tx> block;
};

class message_visitor
{
public:
    virtual void keepalive (germ::keepalive const &) = 0;
    virtual void publish (germ::publish const &) = 0;
    virtual void confirm_req (germ::confirm_req const &) = 0;
    virtual void confirm_ack (germ::confirm_ack const &) = 0;
    virtual void bulk_pull (germ::bulk_pull const &) = 0;
    virtual void bulk_pull_blocks (germ::bulk_pull_blocks const &) = 0;
    virtual void bulk_push (germ::bulk_push const &) = 0;
    virtual void frontier_req (germ::frontier_req const &) = 0;
    virtual void node_id_handshake (germ::node_id_handshake const &) = 0;
    virtual void epoch_req (germ::epoch_req const &) = 0;
    virtual void epoch_bulk_pull (germ::epoch_bulk_pull const &) = 0;
    virtual void epoch_bulk_push (germ::epoch_bulk_push const &) = 0;
    virtual void transaction (germ::transaction_message const &) = 0;
    
    virtual ~message_visitor ();
};

/**
 * Returns seconds passed since unix epoch (posix time)
 */
inline uint64_t seconds_since_epoch ()
{
    return std::chrono::duration_cast<std::chrono::seconds> (std::chrono::system_clock::now ().time_since_epoch ()).count ();
}
}
