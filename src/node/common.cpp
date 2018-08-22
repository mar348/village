
#include <src/node/common.hpp>

#include <src/lib/work.hpp>
#include <src/node/wallet.hpp>

std::array<uint8_t, 2> constexpr germ::message_header::magic_number;
size_t constexpr germ::message_header::ipv4_only_position;
size_t constexpr germ::message_header::bootstrap_server_position;
std::bitset<16> constexpr germ::message_header::block_type_mask;


germ::message_header::message_header (germ::message_type type_a, size_t body_size_a) :
        version (germ::protocol_version),
        type (type_a),
        body_size (body_size_a)
{
}

germ::message_header::message_header (bool & error_a, germ::stream & stream_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

void germ::message_header::serialize (germ::stream & stream_a)
{
    germ::write (stream_a, germ::message_header::magic_number);
    germ::write (stream_a, version);
    germ::write (stream_a, type);
    germ::write (stream_a, static_cast<uint16_t> (extensions.to_ullong ()));
    germ::write (stream_a, body_size);
}

bool germ::message_header::deserialize (germ::stream & stream_a)
{
    uint16_t extensions_l;
    std::array<uint8_t, 2> magic_number_l;
    auto result (germ::read (stream_a, magic_number_l));
    result = result || magic_number_l != magic_number;
    result = result || germ::read (stream_a, version);
    result = result || germ::read (stream_a, type);
    result = result || germ::read (stream_a, extensions_l);
    result = result || germ::read (stream_a, body_size);

    if (!result)
    {
        extensions = extensions_l;
    }
    return result;
}

germ::message::message (germ::message_type type_a, size_t size_a) : header (type_a, size_a)
{
}

germ::message::message (germ::message_header const & header_a) :
header (header_a)
{
}

germ::block_type germ::message_header::block_type () const
{
    return static_cast<germ::block_type> (((extensions & block_type_mask) >> 8).to_ullong ());
}

void germ::message_header::block_type_set (germ::block_type type_a)
{
    extensions &= ~block_type_mask;
    extensions |= std::bitset<16> (static_cast<unsigned long long> (type_a) << 8);
}

bool germ::message_header::ipv4_only ()
{
    return extensions.test (ipv4_only_position);
}

void germ::message_header::ipv4_only_set (bool value_a)
{
    extensions.set (ipv4_only_position, value_a);
}

germ::message_parser::message_parser (germ::message_visitor & visitor_a, germ::work_pool & pool_a) :
visitor (visitor_a),
pool (pool_a),
status (parse_status::success)
{
}

void germ::message_parser::deserialize_buffer (uint8_t const * buffer_a, size_t size_a)
{
    status = parse_status::success;
    germ::bufferstream stream (buffer_a, size_a);
    auto error (false);
    germ::message_header header (error, stream);
    if (error)
    {
        status = parse_status::invalid_header;
        return;
    }

    switch (header.type)
    {
        case germ::message_type::keepalive:
        {
            deserialize_keepalive (stream, header);
            break;
        }
        case germ::message_type::publish:
        {
            deserialize_publish (stream, header);
            break;
        }
        case germ::message_type::confirm_req:
        {
            deserialize_confirm_req (stream, header);
            break;
        }
        case germ::message_type::confirm_ack:
        {
            deserialize_confirm_ack (stream, header);
            break;
        }
        case germ::message_type::node_id_handshake:
        {
            deserialize_node_id_handshake (stream, header);
            break;
        }
        case germ::message_type::epoch_req:
        {
            deserialize_epoch_req(stream, header);
            break;
        }
        case germ::message_type::epoch_bulk_pull:
        {
            deserialize_epoch_bulk_pull(stream, header);
            break;
        }
        case germ::message_type::epoch_bulk_push:
        {
            deserialize_epoch_bulk_push(stream, header);
            break;
        }
        case germ::message_type::transaction:
        {
            deserialize_transaction(stream, header);
            break;
        }
        default:
        {
            status = parse_status::invalid_message_type;
            break;
        }
    }
}

void germ::message_parser::deserialize_keepalive (germ::stream & stream_a, germ::message_header const & header_a)
{
    auto error (false);
    germ::keepalive incoming (error, stream_a, header_a);
    if (!error && at_end (stream_a))
    {
        visitor.keepalive (incoming);
    }
    else
    {
        status = parse_status::invalid_keepalive_message;
    }
}

void germ::message_parser::deserialize_publish (germ::stream & stream_a, germ::message_header const & header_a)
{
    auto error (false);
    germ::publish incoming (error, stream_a, header_a);
    if (!error && at_end (stream_a))
    {
//        if (germ::work_validate (*incoming.block))
//        {
//            visitor.publish (incoming);
//        }
//        else
//        {
//            status = parse_status::insufficient_work;
//        }

        visitor.publish (incoming);
    }
    else
    {
        status = parse_status::invalid_publish_message;
    }
}

void germ::message_parser::deserialize_confirm_req (germ::stream & stream_a, germ::message_header const & header_a)
{
    auto error (false);
    germ::confirm_req incoming (error, stream_a, header_a);
    if (!error && at_end (stream_a))
    {
//        if (germ::work_validate (*incoming.block))
//        {
//            visitor.confirm_req (incoming);
//        }
//        else
//        {
//            status = parse_status::insufficient_work;
//        }

        visitor.confirm_req (incoming);
    }
    else
    {
        status = parse_status::invalid_confirm_req_message;
    }
}

void germ::message_parser::deserialize_confirm_ack (germ::stream & stream_a, germ::message_header const & header_a)
{
    auto error (false);
    germ::confirm_ack incoming (error, stream_a, header_a);
    if (!error && at_end (stream_a))
    {
//        if (germ::work_validate (*incoming.vote->block))
//        {
//            visitor.confirm_ack (incoming);
//        }
//        else
//        {
//            status = parse_status::insufficient_work;
//        }

        visitor.confirm_ack (incoming);
    }
    else
    {
        status = parse_status::invalid_confirm_ack_message;
    }
}

void germ::message_parser::deserialize_node_id_handshake (germ::stream & stream_a, germ::message_header const & header_a)
{
    bool error_l;
    germ::node_id_handshake incoming (error_l, stream_a, header_a);
    if (!error_l && at_end (stream_a))
    {
        visitor.node_id_handshake (incoming);
    }
    else
    {
        status = parse_status::invalid_node_id_handshake_message;
    }
}

void germ::message_parser::deserialize_epoch_req(germ::stream &stream_r, germ::message_header const &header_r)
{
    bool error_r;
    germ::epoch_req incoming (error_r, stream_r, header_r);
    if ( !error_r && at_end(stream_r))
    {
        visitor.epoch_req(incoming);
    }
    else
    {
        status = parse_status ::invalid_epoch_req_message;
    }
}

void germ::message_parser::deserialize_epoch_bulk_pull(germ::stream &stream_r, germ::message_header const &header_r)
{
    bool error_r;
    germ::epoch_bulk_pull incoming (error_r, stream_r, header_r);
    if ( !error_r && at_end(stream_r))
    {
        visitor.epoch_bulk_pull(incoming);
    }
    else
    {
        status = parse_status ::invalid_epoch_bulk_pull_message;
    }
}

void germ::message_parser::deserialize_epoch_bulk_push(germ::stream &stream_r, germ::message_header const &header_r)
{
    bool error_r;
    germ::epoch_bulk_push incoming (header_r);
    if ( !error_r && at_end(stream_r))
    {
        visitor.epoch_bulk_push(incoming);
    }
    else
    {
        status = parse_status ::invalid_epoch_bulk_push_message;
    }
}

void germ::message_parser::deserialize_transaction(germ::stream &stream_r, germ::message_header const &header_r)
{
    bool error_r;
    germ::transaction_message incoming(error_r, stream_r, header_r);
    if (!error_r && at_end(stream_r))
    {
        visitor.transaction(incoming);
    }
    else
    {
        status = parse_status::invalid_transaction_message;
    }
}

bool germ::message_parser::at_end (germ::stream & stream_a)
{
    uint8_t junk;
    auto end (germ::read (stream_a, junk));
    return end;
}

germ::keepalive::keepalive () :
message (germ::message_type::keepalive, 0)
{
    germ::endpoint endpoint (boost::asio::ip::address_v6{}, 0);
    for (auto i (peers.begin ()), n (peers.end ()); i != n; ++i)
    {
        *i = endpoint;
    }
}

germ::keepalive::keepalive (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

void germ::keepalive::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.keepalive (*this);
}

void germ::keepalive::serialize (germ::stream & stream_a)
{
    header.serialize (stream_a);
    for (auto i (peers.begin ()), j (peers.end ()); i != j; ++i)
    {
        assert (i->address ().is_v6 ());
        auto bytes (i->address ().to_v6 ().to_bytes ());
        write (stream_a, bytes);
        write (stream_a, i->port ());
    }
}

bool germ::keepalive::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::keepalive);
    auto error (false);
    for (auto i (peers.begin ()), j (peers.end ()); i != j && !error; ++i)
    {
        std::array<uint8_t, 16> address;
        uint16_t port;
        if (!read (stream_a, address) && !read (stream_a, port))
        {
            *i = germ::endpoint (boost::asio::ip::address_v6 (address), port);
        }
        else
        {
            error = true;
        }
    }
    return error;
}

bool germ::keepalive::operator== (germ::keepalive const & other_a) const
{
    return peers == other_a.peers;
}

germ::publish::publish (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

germ::publish::publish (std::shared_ptr<germ::tx> block_a) :
message (germ::message_type::publish, 0),
block (block_a)
{
    header.block_type_set (block->type ());
}

bool germ::publish::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::publish);
    block = germ::deserialize_block (stream_a, header.block_type ());
    auto result (block == nullptr);
    return result;
}

void germ::publish::serialize (germ::stream & stream_a)
{
    assert (block != nullptr);
    header.serialize (stream_a);
    block->serialize (stream_a);
}

void germ::publish::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.publish (*this);
}

bool germ::publish::operator== (germ::publish const & other_a) const
{
    return *block == *other_a.block;
}

germ::confirm_req::confirm_req (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

germ::confirm_req::confirm_req (std::shared_ptr<germ::tx> block_a) :
message (germ::message_type::confirm_req, 0),
block (block_a)
{
    header.block_type_set (block->type ());
}

bool germ::confirm_req::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::confirm_req);
    block = germ::deserialize_block (stream_a, header.block_type ());
    auto result (block == nullptr);
    return result;
}

void germ::confirm_req::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.confirm_req (*this);
}

void germ::confirm_req::serialize (germ::stream & stream_a)
{
    assert (block != nullptr);
    header.serialize (stream_a);
    block->serialize (stream_a);
}

bool germ::confirm_req::operator== (germ::confirm_req const & other_a) const
{
    return *block == *other_a.block;
}

germ::confirm_ack::confirm_ack (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a),
vote (std::make_shared<germ::vote> (error_a, stream_a, header.block_type ()))
{
}

germ::confirm_ack::confirm_ack (std::shared_ptr<germ::vote> vote_a) :
message (germ::message_type::confirm_ack, 0),
vote (vote_a)
{
    header.block_type_set (vote->block->type ());
}

bool germ::confirm_ack::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::confirm_ack);
    auto result (vote->deserialize (stream_a));
    return result;
}

void germ::confirm_ack::serialize (germ::stream & stream_a)
{
    assert (header.block_type () == germ::block_type::send || header.block_type () == germ::block_type::receive || header.block_type () == germ::block_type::open || header.block_type () == germ::block_type::change || header.block_type () == germ::block_type::state);
    header.serialize (stream_a);
    vote->serialize (stream_a, header.block_type ());
}

bool germ::confirm_ack::operator== (germ::confirm_ack const & other_a) const
{
    auto result (*vote == *other_a.vote);
    return result;
}

void germ::confirm_ack::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.confirm_ack (*this);
}

germ::frontier_req::frontier_req () :
message (germ::message_type::frontier_req, 0)
{
}

germ::frontier_req::frontier_req (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

bool germ::frontier_req::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::frontier_req);
    auto result (read (stream_a, start.bytes));
    if (!result)
    {
        result = read (stream_a, age);
        if (!result)
        {
            result = read (stream_a, count);
        }
    }
    return result;
}

void germ::frontier_req::serialize (germ::stream & stream_a)
{
    header.serialize (stream_a);
    write (stream_a, start.bytes);
    write (stream_a, age);
    write (stream_a, count);
}

void germ::frontier_req::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.frontier_req (*this);
}

bool germ::frontier_req::operator== (germ::frontier_req const & other_a) const
{
    return start == other_a.start && age == other_a.age && count == other_a.count;
}

germ::bulk_pull::bulk_pull () :
message (germ::message_type::bulk_pull, 0)
{
}

germ::bulk_pull::bulk_pull (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

void germ::bulk_pull::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.bulk_pull (*this);
}

bool germ::bulk_pull::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::bulk_pull);
    auto result (read (stream_a, start));
    if (!result)
    {
        result = read (stream_a, end);
    }
    return result;
}

void germ::bulk_pull::serialize (germ::stream & stream_a)
{
    header.serialize (stream_a);
    write (stream_a, start);
    write (stream_a, end);
}

germ::bulk_pull_blocks::bulk_pull_blocks () :
message (germ::message_type::bulk_pull_blocks, 0)
{
}

germ::bulk_pull_blocks::bulk_pull_blocks (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a)
{
    if (!error_a)
    {
        error_a = deserialize (stream_a);
    }
}

void germ::bulk_pull_blocks::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.bulk_pull_blocks (*this);
}

bool germ::bulk_pull_blocks::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::bulk_pull_blocks);
    auto result (read (stream_a, min_hash));
    if (result)
        return result;

    result = read (stream_a, max_hash);
    if (result)
        return result;

    result = read (stream_a, mode);
    if (result)
        return result;

    result = read (stream_a, max_count);
    return result;
}

void germ::bulk_pull_blocks::serialize (germ::stream & stream_a)
{
    header.serialize (stream_a);
    write (stream_a, min_hash);
    write (stream_a, max_hash);
    write (stream_a, mode);
    write (stream_a, max_count);
}

germ::bulk_push::bulk_push () :
message (germ::message_type::bulk_push, 0)
{
}

germ::bulk_push::bulk_push (germ::message_header const & header_a) :
message (header_a)
{
}

bool germ::bulk_push::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::bulk_push);
    return false;
}

void germ::bulk_push::serialize (germ::stream & stream_a)
{
    header.serialize (stream_a);
}

void germ::bulk_push::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.bulk_push (*this);
}

size_t constexpr germ::node_id_handshake::query_flag;
size_t constexpr germ::node_id_handshake::response_flag;

germ::node_id_handshake::node_id_handshake (bool & error_a, germ::stream & stream_a, germ::message_header const & header_a) :
message (header_a),
query (boost::none),
response (boost::none)
{
    error_a = deserialize (stream_a);
}

germ::node_id_handshake::node_id_handshake (boost::optional<germ::uint256_union> query, boost::optional<std::pair<germ::account, germ::signature>> response) :
message (germ::message_type::node_id_handshake, 0),
query (query),
response (response)
{
    if (query)
    {
        header.extensions.set (query_flag);
    }
    if (response)
    {
        header.extensions.set (response_flag);
    }
}

bool germ::node_id_handshake::deserialize (germ::stream & stream_a)
{
    auto result (false);
    assert (header.type == germ::message_type::node_id_handshake);
    if (!result && header.extensions.test (query_flag))
    {
        germ::uint256_union query_hash;
        result = read (stream_a, query_hash);
        if (!result)
        {
            query = query_hash;
        }
    }
    if (result || !header.extensions.test (response_flag))
        return result;

    germ::account response_account;
    result = read (stream_a, response_account);
    if (result)
        return result;

    germ::signature response_signature;
    result = read (stream_a, response_signature);
    if (result)
        return result;

    response = std::make_pair (response_account, response_signature);
    return result;
}

void germ::node_id_handshake::serialize (germ::stream & stream_a)
{
    header.serialize (stream_a);
    if (query)
    {
        write (stream_a, *query);
    }
    if (response)
    {
        write (stream_a, response->first);
        write (stream_a, response->second);
    }
}

bool germ::node_id_handshake::operator== (germ::node_id_handshake const & other_a) const
{
    auto result (*query == *other_a.query && *response == *other_a.response);
    return result;
}

void germ::node_id_handshake::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.node_id_handshake (*this);
}

germ::message_visitor::~message_visitor ()
{
}

germ::epoch_req::epoch_req()
:message(germ::message_type::epoch_req, 0)
{

}

germ::epoch_req::epoch_req(bool &error_r, germ::stream &stream_r, germ::message_header const &header_r)
:message(header_r)
{
    if (!error_r)
    {
        error_r = deserialize (stream_r);
    }
}

bool germ::epoch_req::deserialize(germ::stream &stream_r)
{
    assert(header.type == germ::message_type::epoch_req);
    auto result (read (stream_r, start.bytes));
    if (!result)
    {
        result = read (stream_r, age);
        if (!result)
        {
            result = read (stream_r, count);
        }
    }
    return result;
}

void germ::epoch_req::serialize(germ::stream &stream_r)
{
    header.serialize(stream_r);
    write (stream_r, start.bytes);
    write (stream_r, age);
    write (stream_r, count);
}

bool germ::epoch_req::operator==(germ::epoch_req const &epoch) const
{
    return start == epoch.start && age == epoch.age && count == epoch.count;
}

void germ::epoch_req::visit(germ::message_visitor &visit_r) const
{
    visit_r.epoch_req(*this);
}

germ::epoch_bulk_pull::epoch_bulk_pull()
:message(germ::message_type::epoch_bulk_pull, 0)
{

}

germ::epoch_bulk_pull::epoch_bulk_pull(bool & error_r, germ::stream & stream_r, germ::message_header const & header_r)
:message(header_r)
{
    if (error_r)
    {
        error_r = deserialize(stream_r);
    }
}

void germ::epoch_bulk_pull::serialize(germ::stream & stream_r)
{
    header.serialize(stream_r);
    write(stream_r, start);
    write(stream_r, end);
}

bool germ::epoch_bulk_pull::deserialize(germ::stream & stream_r)
{
    assert(header.type == germ::message_type::epoch_bulk_pull);
    auto result(germ::read(stream_r, start));
    if (result)
    {
        result = germ::read(stream_r, end);
    }

    return result;

}

void germ::epoch_bulk_pull::visit(germ::message_visitor & visit_r) const
{
    visit_r.epoch_bulk_pull(*this);
}

germ::epoch_bulk_push::epoch_bulk_push()
:message(germ::message_type::epoch_bulk_push, 0)
{
}

germ::epoch_bulk_push::epoch_bulk_push(germ::message_header const & header_r)
:message(header_r)
{
}

void germ::epoch_bulk_push::serialize(germ::stream & stream_r)
{
    header.serialize(stream_r);
}

bool germ::epoch_bulk_push::deserialize(germ::stream & stream_r)
{
    assert(header.type == germ::message_type::epoch_bulk_push);
    return false;
}

void germ::epoch_bulk_push::visit(germ::message_visitor & visit_r) const
{
    visit_r.epoch_bulk_push(*this);
}

germ::transaction_message::transaction_message(bool & error_r, germ::stream & stream_r, germ::message_header const & header_r)
:message(header_r)
{
    if (!error_r)
    {
        error_r = deserialize (stream_r);
    }
}

germ::transaction_message::transaction_message(std::shared_ptr<germ::tx> block_r)
:message(germ::message_type::transaction, 0),
block (block_r)
{
    header.block_type_set (block->type ());
}

bool germ::transaction_message::deserialize (germ::stream & stream_a)
{
    assert (header.type == germ::message_type::transaction);
    block = germ::deserialize_block (stream_a, header.block_type ());
    auto result (block == nullptr);
    return result;
}

void germ::transaction_message::serialize (germ::stream & stream_a)
{
    assert (block != nullptr);
    header.serialize (stream_a);
    block->serialize (stream_a);
}

void germ::transaction_message::visit (germ::message_visitor & visitor_a) const
{
    visitor_a.transaction (*this);
}

bool germ::transaction_message::operator== (germ::transaction_message const & other_a) const
{
    return *block == *other_a.block;
}