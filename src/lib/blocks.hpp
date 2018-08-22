#pragma once

#include <src/lib/numbers.hpp>

#include <assert.h>
#include <blake2/blake2.h>
#include <boost/property_tree/json_parser.hpp>
#include <streambuf>

namespace germ
{
class tx;
std::string to_string(std::vector<uint8_t> const & vector);
void to_vector(std::vector<uint8_t> & vec, std::string const & str);

std::string to_string_hex (uint64_t);
bool from_string_hex (std::string const &, uint64_t &);
// We operate on streams of uint8_t by convention
using stream = std::basic_streambuf<uint8_t>;
// Read a raw byte stream the size of `T' and fill value.
bool read_data (germ::stream & stream_a, std::vector<uint8_t> & value);
void write_data (germ::stream & stream_a, std::vector<uint8_t> const & value);

template <typename T>
bool read (germ::stream & stream_a, T & value)
{
    static_assert (std::is_pod<T>::value, "Can't stream read non-standard layout types");
    auto amount_read (stream_a.sgetn (reinterpret_cast<uint8_t *> (&value), sizeof (value)));
    return amount_read != sizeof (value);
}
template <typename T>
void write (germ::stream & stream_a, T const & value)
{
    static_assert (std::is_pod<T>::value, "Can't stream write non-standard layout types");
    auto amount_written (stream_a.sputn (reinterpret_cast<uint8_t const *> (&value), sizeof (value)));
    assert (amount_written == sizeof (value));
}
class block_visitor;
enum class block_type : uint8_t
{
    invalid = 0,
    not_a_block = 1,
    send = 2,
    receive = 3,
    open = 4,
    change = 5,
    state = 6,
    epoch = 7,
    not_an_epoch = 8,
    vote = 9
};
class block
{
public:
    // Return a digest of the hashables in this block.
    germ::block_hash hash () const;
    std::string to_json ();
    virtual void hash (blake2b_state &) const = 0;
    virtual uint64_t block_work () const = 0;
    virtual void block_work_set (uint64_t) = 0;
    // Previous block in account's chain, zero for open block
    virtual germ::block_hash previous () const = 0;
    // Source block for open/receive blocks, zero otherwise.
    virtual germ::block_hash source () const = 0;
    // Previous block or account number for open blocks
    virtual germ::block_hash root () const = 0;
    virtual germ::account representative () const = 0;
    virtual void serialize (germ::stream &) const = 0;
    virtual void serialize_json (std::string &) const = 0;
    virtual void visit (germ::block_visitor &) const = 0;
    virtual bool operator== (germ::block const &) const = 0;
    virtual germ::block_type type () const = 0;
    virtual germ::signature block_signature () const = 0;
    virtual void signature_set (germ::uint512_union const &) = 0;
    virtual ~block () = default;
    virtual bool valid_predecessor (germ::block const &) const = 0;
};
class send_hashables
{
public:
    send_hashables (germ::account const &, germ::block_hash const &, germ::amount const &);
    send_hashables (bool &, germ::stream &);
    send_hashables (bool &, boost::property_tree::ptree const &);
    void hash (blake2b_state &) const;
    germ::block_hash previous;
    germ::account destination;
    germ::amount balance;
};
class send_block : public germ::block
{
public:
    send_block (germ::block_hash const &, germ::account const &, germ::amount const &, germ::raw_key const &, germ::public_key const &, uint64_t);
    send_block (bool &, germ::stream &);
    send_block (bool &, boost::property_tree::ptree const &);
    virtual ~send_block () = default;
    using germ::block::hash;
    void hash (blake2b_state &) const override;
    uint64_t block_work () const override;
    void block_work_set (uint64_t) override;
    germ::block_hash previous () const override;
    germ::block_hash source () const override;
    germ::block_hash root () const override;
    germ::account representative () const override;
    void serialize (germ::stream &) const override;
    void serialize_json (std::string &) const override;
    bool deserialize (germ::stream &);
    bool deserialize_json (boost::property_tree::ptree const &);
    void visit (germ::block_visitor &) const override;
    germ::block_type type () const override;
    germ::signature block_signature () const override;
    void signature_set (germ::uint512_union const &) override;
    bool operator== (germ::block const &) const override;
    bool operator== (germ::send_block const &) const;
    bool valid_predecessor (germ::block const &) const override;
    static size_t constexpr size = sizeof (germ::account) + sizeof (germ::block_hash) + sizeof (germ::amount) + sizeof (germ::signature) + sizeof (uint64_t);
    send_hashables hashables;
    germ::signature signature;
    uint64_t work;
};
class receive_hashables
{
public:
    receive_hashables (germ::block_hash const &, germ::block_hash const &);
    receive_hashables (bool &, germ::stream &);
    receive_hashables (bool &, boost::property_tree::ptree const &);
    void hash (blake2b_state &) const;
    germ::block_hash previous;
    germ::block_hash source;
};
class receive_block : public germ::block
{
public:
    receive_block (germ::block_hash const &, germ::block_hash const &, germ::raw_key const &, germ::public_key const &, uint64_t);
    receive_block (bool &, germ::stream &);
    receive_block (bool &, boost::property_tree::ptree const &);
    virtual ~receive_block () = default;
    using germ::block::hash;
    void hash (blake2b_state &) const override;
    uint64_t block_work () const override;
    void block_work_set (uint64_t) override;
    germ::block_hash previous () const override;
    germ::block_hash source () const override;
    germ::block_hash root () const override;
    germ::account representative () const override;
    void serialize (germ::stream &) const override;
    void serialize_json (std::string &) const override;
    bool deserialize (germ::stream &);
    bool deserialize_json (boost::property_tree::ptree const &);
    void visit (germ::block_visitor &) const override;
    germ::block_type type () const override;
    germ::signature block_signature () const override;
    void signature_set (germ::uint512_union const &) override;
    bool operator== (germ::block const &) const override;
    bool operator== (germ::receive_block const &) const;
    bool valid_predecessor (germ::block const &) const override;
    static size_t constexpr size = sizeof (germ::block_hash) + sizeof (germ::block_hash) + sizeof (germ::signature) + sizeof (uint64_t);
    receive_hashables hashables;
    germ::signature signature;
    uint64_t work;
};
class open_hashables
{
public:
    open_hashables (germ::block_hash const &, /*germ::account const &,*/ germ::account const &);
    open_hashables (bool &, germ::stream &);
    open_hashables (bool &, boost::property_tree::ptree const &);
    void hash (blake2b_state &) const;
    germ::block_hash source;
//    germ::account representative;
    germ::account account;
};
class open_block : public germ::block
{
public:
    open_block (germ::block_hash const &, /*germ::account const &,*/ germ::account const &, germ::raw_key const &, germ::public_key const &, uint64_t);
    open_block (germ::block_hash const &, /*germ::account const &,*/ germ::account const &, std::nullptr_t);
    open_block (bool &, germ::stream &);
    open_block (bool &, boost::property_tree::ptree const &);
    virtual ~open_block () = default;
    using germ::block::hash;
    void hash (blake2b_state &) const override;
    uint64_t block_work () const override;
    void block_work_set (uint64_t) override;
    germ::block_hash previous () const override;
    germ::block_hash source () const override;
    germ::block_hash root () const override;
    germ::account representative () const override;
    void serialize (germ::stream &) const override;
    void serialize_json (std::string &) const override;
    bool deserialize (germ::stream &);
    bool deserialize_json (boost::property_tree::ptree const &);
    void visit (germ::block_visitor &) const override;
    germ::block_type type () const override;
    germ::signature block_signature () const override;
    void signature_set (germ::uint512_union const &) override;
    bool operator== (germ::block const &) const override;
    bool operator== (germ::open_block const &) const;
    bool valid_predecessor (germ::block const &) const override;
    static size_t constexpr size = sizeof (germ::block_hash) + sizeof (germ::account) + sizeof (germ::account) + sizeof (germ::signature) + sizeof (uint64_t);
    germ::open_hashables hashables;
    germ::signature signature;
    uint64_t work;
};
class change_hashables
{
public:
    change_hashables (germ::block_hash const &/*, germ::account const &*/);
    change_hashables (bool &, germ::stream &);
    change_hashables (bool &, boost::property_tree::ptree const &);
    void hash (blake2b_state &) const;
    germ::block_hash previous;
//    germ::account representative;
};
class change_block : public germ::block
{
public:
    change_block (germ::block_hash const &, /*germ::account const &,*/ germ::raw_key const &, germ::public_key const &, uint64_t);
    change_block (bool &, germ::stream &);
    change_block (bool &, boost::property_tree::ptree const &);
    virtual ~change_block () = default;
    using germ::block::hash;
    void hash (blake2b_state &) const override;
    uint64_t block_work () const override;
    void block_work_set (uint64_t) override;
    germ::block_hash previous () const override;
    germ::block_hash source () const override;
    germ::block_hash root () const override;
    germ::account representative () const override;
    void serialize (germ::stream &) const override;
    void serialize_json (std::string &) const override;
    bool deserialize (germ::stream &);
    bool deserialize_json (boost::property_tree::ptree const &);
    void visit (germ::block_visitor &) const override;
    germ::block_type type () const override;
    germ::signature block_signature () const override;
    void signature_set (germ::uint512_union const &) override;
    bool operator== (germ::block const &) const override;
    bool operator== (germ::change_block const &) const;
    bool valid_predecessor (germ::block const &) const override;
    static size_t constexpr size = sizeof (germ::block_hash) + sizeof (germ::account) + sizeof (germ::signature) + sizeof (uint64_t);
    germ::change_hashables hashables;
    germ::signature signature;
    uint64_t work;
};
class state_hashables
{
public:
    state_hashables (germ::account const &, germ::block_hash const &, /*germ::account const &,*/ germ::amount const &, germ::uint256_union const &);
    state_hashables (bool &, germ::stream &);
    state_hashables (bool &, boost::property_tree::ptree const &);
    void hash (blake2b_state &) const;
    // Account# / public key that operates this account
    // Uses:
    // Bulk signature validation in advance of further ledger processing
    // Arranging uncomitted transactions by account
    germ::account account;
    // Previous transaction in this chain
    germ::block_hash previous;
    // Representative of this account
//    germ::account representative;
    // Current balance of this account
    // Allows lookup of account balance simply by looking at the head block
    germ::amount balance;
    // Link field contains source block_hash if receiving, destination account if sending
    germ::uint256_union link;
};
class state_block : public germ::block
{
public:
    state_block (germ::account const &, germ::block_hash const &, /*germ::account const &,*/ germ::amount const &, germ::uint256_union const &, germ::raw_key const &, germ::public_key const &, uint64_t);
    state_block (bool &, germ::stream &);
    state_block (bool &, boost::property_tree::ptree const &);
    virtual ~state_block () = default;
    using germ::block::hash;
    void hash (blake2b_state &) const override;
    uint64_t block_work () const override;
    void block_work_set (uint64_t) override;
    germ::block_hash previous () const override;
    germ::block_hash source () const override;
    germ::block_hash root () const override;
    germ::account representative () const override;
    void serialize (germ::stream &) const override;
    void serialize_json (std::string &) const override;
    bool deserialize (germ::stream &);
    bool deserialize_json (boost::property_tree::ptree const &);
    void visit (germ::block_visitor &) const override;
    germ::block_type type () const override;
    germ::signature block_signature () const override;
    void signature_set (germ::uint512_union const &) override;
    bool operator== (germ::block const &) const override;
    bool operator== (germ::state_block const &) const;
    bool valid_predecessor (germ::block const &) const override;
    static size_t constexpr size = sizeof (germ::account) + sizeof (germ::block_hash) + sizeof (germ::account) + sizeof (germ::amount) + sizeof (germ::uint256_union) + sizeof (germ::signature) + sizeof (uint64_t);
    germ::state_hashables hashables;
    germ::signature signature;
    uint64_t work;
};
class block_visitor
{
public:
    virtual void send_block (germ::send_block const &) = 0;
    virtual void receive_block (germ::receive_block const &) = 0;
    virtual void open_block (germ::open_block const &) = 0;
    virtual void change_block (germ::change_block const &) = 0;
    virtual void state_block (germ::state_block const &) = 0;
    virtual ~block_visitor () = default;
    virtual void tx (germ::tx const &) = 0;
};
std::unique_ptr<germ::tx> deserialize_block (germ::stream &);
std::unique_ptr<germ::tx> deserialize_block (germ::stream &, germ::block_type);
std::unique_ptr<germ::tx> deserialize_block_json (boost::property_tree::ptree const &);
void serialize_block (germ::stream &, germ::tx const &);
}
