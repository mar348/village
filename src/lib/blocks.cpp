#include <src/lib/blocks.hpp>

#include <boost/endian/conversion.hpp>
#include <src/lib/tx.h>

/** Compare blocks, first by type, then content. This is an optimization over dynamic_cast, which is very slow on some platforms. */
namespace
{
template <typename T>
bool blocks_equal (T const & first, germ::block const & second)
{
    static_assert (std::is_base_of<germ::block, T>::value, "Input parameter is not a block type");
    return (first.type () == second.type ()) && (static_cast<T const &> (second)) == first;
}
}

std::string germ::to_string(std::vector<uint8_t> const & vector)
{
    std::string str;

    for (int i = 0; i < (int)vector.size(); ++i)
        str.push_back(vector[i]);

    return str;
}

void germ::to_vector(std::vector<uint8_t> & vec, std::string const & str)
{
    std::vector<uint8_t>().swap(vec);
    for (auto i : str)
        vec.push_back(i);
}


bool germ::read_data (germ::stream & stream_a, std::vector<uint8_t> & value)
{
    int size = sizeof(uint8_t) * value.size();
//    static_assert (std::is_pod<T>::value, "Can't stream read non-standard layout types");
    auto amount_read (stream_a.sgetn (reinterpret_cast<uint8_t *> (&value[0]), size));
    return amount_read != size;
}
void germ::write_data (germ::stream & stream_a, std::vector<uint8_t> const & value)
{
//    static_assert (std::is_pod<T>::value, "Can't stream write non-standard layout types");
    auto amount_written (stream_a.sputn (reinterpret_cast<uint8_t const *> (&value[0]), sizeof(uint8_t) * value.size()));
    assert (amount_written == sizeof(uint8_t) * value.size());
}

std::string germ::to_string_hex (uint64_t value_a)
{
    std::stringstream stream;
    stream << std::hex << std::noshowbase << std::setw (16) << std::setfill ('0');
    stream << value_a;
    return stream.str ();
}

bool germ::from_string_hex (std::string const & value_a, uint64_t & target_a)
{
    auto error (value_a.empty ());
    if (!error)
    {
        error = value_a.size () > 16;
        if (!error)
        {
            std::stringstream stream (value_a);
            stream << std::hex << std::noshowbase;
            try
            {
                uint64_t number_l;
                stream >> number_l;
                target_a = number_l;
                if (!stream.eof ())
                {
                    error = true;
                }
            }
            catch (std::runtime_error &)
            {
                error = true;
            }
        }
    }
    return error;
}

std::string germ::block::to_json ()
{
    std::string result;
    serialize_json (result);
    return result;
}

germ::block_hash germ::block::hash () const
{
    germ::uint256_union result;
    blake2b_state hash_l;
    auto status (blake2b_init (&hash_l, sizeof (result.bytes)));
    assert (status == 0);
    hash (hash_l);
    status = blake2b_final (&hash_l, result.bytes.data (), sizeof (result.bytes));
    assert (status == 0);
    return result;
}

void germ::send_block::visit (germ::block_visitor & visitor_a) const
{
    visitor_a.send_block (*this);
}

void germ::send_block::hash (blake2b_state & hash_a) const
{
    hashables.hash (hash_a);
}

uint64_t germ::send_block::block_work () const
{
    return work;
}

void germ::send_block::block_work_set (uint64_t work_a)
{
    work = work_a;
}

germ::send_hashables::send_hashables (germ::block_hash const & previous_a, germ::account const & destination_a, germ::amount const & balance_a) :
previous (previous_a),
destination (destination_a),
balance (balance_a)
{
}

germ::send_hashables::send_hashables (bool & error_a, germ::stream & stream_a)
{
    error_a = germ::read (stream_a, previous.bytes);
    if (error_a)
        return;

    error_a = germ::read (stream_a, destination.bytes);
    if (error_a)
        return;

    error_a = germ::read (stream_a, balance.bytes);
}

germ::send_hashables::send_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
    try
    {
        auto previous_l (tree_a.get<std::string> ("previous"));
        auto destination_l (tree_a.get<std::string> ("destination"));
        auto balance_l (tree_a.get<std::string> ("balance"));
        error_a = previous.decode_hex (previous_l);
        if (error_a)
            return;

        error_a = destination.decode_account (destination_l);
        if (error_a)
            return;

        error_a = balance.decode_hex (balance_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::send_hashables::hash (blake2b_state & hash_a) const
{
    auto status (blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes)));
    assert (status == 0);
    status = blake2b_update (&hash_a, destination.bytes.data (), sizeof (destination.bytes));
    assert (status == 0);
    status = blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
    assert (status == 0);
}

void germ::send_block::serialize (germ::stream & stream_a) const
{
    write (stream_a, hashables.previous.bytes);
    write (stream_a, hashables.destination.bytes);
    write (stream_a, hashables.balance.bytes);
    write (stream_a, signature.bytes);
    write (stream_a, work);
}

void germ::send_block::serialize_json (std::string & string_a) const
{
    boost::property_tree::ptree tree;
    tree.put ("type", "send");
    std::string previous;
    hashables.previous.encode_hex (previous);
    tree.put ("previous", previous);
    tree.put ("destination", hashables.destination.to_account ());
    std::string balance;
    hashables.balance.encode_hex (balance);
    tree.put ("balance", balance);
    std::string signature_l;
    signature.encode_hex (signature_l);
    tree.put ("work", germ::to_string_hex (work));
    tree.put ("signature", signature_l);
    std::stringstream ostream;
    boost::property_tree::write_json (ostream, tree);
    string_a = ostream.str ();
}

bool germ::send_block::deserialize (germ::stream & stream_a)
{
    auto error (false);
    error = read (stream_a, hashables.previous.bytes);
    if (error)
        return error;

    error = read (stream_a, hashables.destination.bytes);
    if (error)
        return error;

    error = read (stream_a, hashables.balance.bytes);
    if (error)
        return error;

    error = read (stream_a, signature.bytes);
    if (error)
        return error;

    error = read (stream_a, work);

    return error;
}

bool germ::send_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto error (false);
    try
    {
        assert (tree_a.get<std::string> ("type") == "send");
        auto previous_l (tree_a.get<std::string> ("previous"));
        auto destination_l (tree_a.get<std::string> ("destination"));
        auto balance_l (tree_a.get<std::string> ("balance"));
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error = hashables.previous.decode_hex (previous_l);
        if (error)
            return error;

        error = hashables.destination.decode_account (destination_l);
        if (error)
            return error;

        error = hashables.balance.decode_hex (balance_l);
        if (error)
            return error;

        error = germ::from_string_hex (work_l, work);
        if (error)
            return error;

        error = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
    return error;
}

germ::send_block::send_block (germ::block_hash const & previous_a, germ::account const & destination_a, germ::amount const & balance_a, germ::raw_key const & prv_a, germ::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, destination_a, balance_a),
signature (germ::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

germ::send_block::send_block (bool & error_a, germ::stream & stream_a) :
hashables (error_a, stream_a)
{
    if (error_a)
        return;

    error_a = germ::read (stream_a, signature.bytes);
    if (error_a)
        return;

    error_a = germ::read (stream_a, work);
}

germ::send_block::send_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
    if (error_a)
        return;

    try
    {
        auto signature_l (tree_a.get<std::string> ("signature"));
        auto work_l (tree_a.get<std::string> ("work"));
        error_a = signature.decode_hex (signature_l);
        if (error_a)
            return;

        error_a = germ::from_string_hex (work_l, work);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

bool germ::send_block::operator== (germ::block const & other_a) const
{
    return blocks_equal (*this, other_a);
}

bool germ::send_block::valid_predecessor (germ::block const & block_a) const
{
    bool result;
    switch (block_a.type ())
    {
        case germ::block_type::send:
        case germ::block_type::receive:
        case germ::block_type::open:
        case germ::block_type::change:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

germ::block_type germ::send_block::type () const
{
    return germ::block_type::send;
}

bool germ::send_block::operator== (germ::send_block const & other_a) const
{
    auto result (hashables.destination == other_a.hashables.destination && hashables.previous == other_a.hashables.previous && hashables.balance == other_a.hashables.balance && work == other_a.work && signature == other_a.signature);
    return result;
}

germ::block_hash germ::send_block::previous () const
{
    return hashables.previous;
}

germ::block_hash germ::send_block::source () const
{
    return 0;
}

germ::block_hash germ::send_block::root () const
{
    return hashables.previous;
}

germ::account germ::send_block::representative () const
{
    return 0;
}

germ::signature germ::send_block::block_signature () const
{
    return signature;
}

void germ::send_block::signature_set (germ::uint512_union const & signature_a)
{
    signature = signature_a;
}

germ::open_hashables::open_hashables (germ::block_hash const & source_a, /*germ::account const & representative_a,*/ germ::account const & account_a) :
source (source_a),
//representative (representative_a),
account (account_a)
{
}

germ::open_hashables::open_hashables (bool & error_a, germ::stream & stream_a)
{
    error_a = germ::read (stream_a, source.bytes);
    if (error_a)
        return ;

//    error_a = germ::read (stream_a, representative.bytes);
//    if (error_a)
//        return ;

    error_a = germ::read (stream_a, account.bytes);
}

germ::open_hashables::open_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
    try
    {
        auto source_l (tree_a.get<std::string> ("source"));
//        auto representative_l (tree_a.get<std::string> ("representative"));
        auto account_l (tree_a.get<std::string> ("account"));
        error_a = source.decode_hex (source_l);
        if (error_a)
            return ;

//        error_a = representative.decode_account (representative_l);
//        if (error_a)
//            return ;

        error_a = account.decode_account (account_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::open_hashables::hash (blake2b_state & hash_a) const
{
    blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
//    blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
    blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
}

germ::open_block::open_block (germ::block_hash const & source_a, /*germ::account const & representative_a,*/ germ::account const & account_a, germ::raw_key const & prv_a, germ::public_key const & pub_a, uint64_t work_a) :
hashables (source_a, /*representative_a,*/ account_a),
signature (germ::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
 //   assert (!representative_a.is_zero ());
}

germ::open_block::open_block (germ::block_hash const & source_a, /*germ::account const & representative_a,*/ germ::account const & account_a, std::nullptr_t) :
hashables (source_a, /*representative_a,*/ account_a),
work (0)
{
    signature.clear ();
}

germ::open_block::open_block (bool & error_a, germ::stream & stream_a) :
hashables (error_a, stream_a)
{
    if (error_a)
        return ;

    error_a = germ::read (stream_a, signature);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, work);
}

germ::open_block::open_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
    if (error_a)
        return ;

    try
    {
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error_a = germ::from_string_hex (work_l, work);
        if (error_a)
            return ;

        error_a = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::open_block::hash (blake2b_state & hash_a) const
{
    hashables.hash (hash_a);
}

uint64_t germ::open_block::block_work () const
{
    return work;
}

void germ::open_block::block_work_set (uint64_t work_a)
{
    work = work_a;
}

germ::block_hash germ::open_block::previous () const
{
    germ::block_hash result (0);
    return result;
}

void germ::open_block::serialize (germ::stream & stream_a) const
{
    write (stream_a, hashables.source);
//    write (stream_a, hashables.representative);
    write (stream_a, hashables.account);
    write (stream_a, signature);
    write (stream_a, work);
}

void germ::open_block::serialize_json (std::string & string_a) const
{
    boost::property_tree::ptree tree;
    tree.put ("type", "open");
    tree.put ("source", hashables.source.to_string ());
//    tree.put ("representative", representative ().to_account ());
    tree.put ("account", hashables.account.to_account ());
    std::string signature_l;
    signature.encode_hex (signature_l);
    tree.put ("work", germ::to_string_hex (work));
    tree.put ("signature", signature_l);
    std::stringstream ostream;
    boost::property_tree::write_json (ostream, tree);
    string_a = ostream.str ();
}

bool germ::open_block::deserialize (germ::stream & stream_a)
{
    auto error (read (stream_a, hashables.source));
    if (error)
        return error;

//    error = read (stream_a, hashables.representative);
//    if (error)
//        return error;

    error = read (stream_a, hashables.account);
    if (error)
        return error;

    error = read (stream_a, signature);
    if (error)
        return error;

    error = read (stream_a, work);

    return error;
}

bool germ::open_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto error (false);
    try
    {
        assert (tree_a.get<std::string> ("type") == "open");
        auto source_l (tree_a.get<std::string> ("source"));
//        auto representative_l (tree_a.get<std::string> ("representative"));
        auto account_l (tree_a.get<std::string> ("account"));
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error = hashables.source.decode_hex (source_l);
        if (error)
            return error;

//        error = hashables.representative.decode_hex (representative_l);
//        if (error)
//            return error;

        error = hashables.account.decode_hex (account_l);
        if (error)
            return error;

        error = germ::from_string_hex (work_l, work);
        if (error)
            return error;

        error = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
    return error;
}

void germ::open_block::visit (germ::block_visitor & visitor_a) const
{
    visitor_a.open_block (*this);
}

germ::block_type germ::open_block::type () const
{
    return germ::block_type::open;
}

bool germ::open_block::operator== (germ::block const & other_a) const
{
    return blocks_equal (*this, other_a);
}

bool germ::open_block::operator== (germ::open_block const & other_a) const
{
    return hashables.source == other_a.hashables.source && /*hashables.representative == other_a.hashables.representative &&*/ hashables.account == other_a.hashables.account && work == other_a.work && signature == other_a.signature;
}

bool germ::open_block::valid_predecessor (germ::block const & block_a) const
{
    return false;
}

germ::block_hash germ::open_block::source () const
{
    return hashables.source;
}

germ::block_hash germ::open_block::root () const
{
    return hashables.account;
}

germ::account germ::open_block::representative () const
{
//    return hashables.representative;
    return 0;
}

germ::signature germ::open_block::block_signature () const
{
    return signature;
}

void germ::open_block::signature_set (germ::uint512_union const & signature_a)
{
    signature = signature_a;
}

germ::change_hashables::change_hashables (germ::block_hash const & previous_a/*, germ::account const & representative_a*/) :
previous (previous_a)
//representative (representative_a)
{
}

germ::change_hashables::change_hashables (bool & error_a, germ::stream & stream_a)
{
    error_a = germ::read (stream_a, previous);
    if (error_a)
        return ;

//    error_a = germ::read (stream_a, representative);
}

germ::change_hashables::change_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
    try
    {
        auto previous_l (tree_a.get<std::string> ("previous"));
//        auto representative_l (tree_a.get<std::string> ("representative"));
        error_a = previous.decode_hex (previous_l);
        if (error_a)
            return ;

//        error_a = representative.decode_account (representative_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::change_hashables::hash (blake2b_state & hash_a) const
{
    blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
//    blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
}

germ::change_block::change_block (germ::block_hash const & previous_a, /*germ::account const & representative_a,*/ germ::raw_key const & prv_a, germ::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a/*, representative_a*/),
signature (germ::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

germ::change_block::change_block (bool & error_a, germ::stream & stream_a) :
hashables (error_a, stream_a)
{
    if (error_a)
        return ;

    error_a = germ::read (stream_a, signature);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, work);
}

germ::change_block::change_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
    if (error_a)
        return ;

    try
    {
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error_a = germ::from_string_hex (work_l, work);
        if (error_a)
            return ;

        error_a = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::change_block::hash (blake2b_state & hash_a) const
{
    hashables.hash (hash_a);
}

uint64_t germ::change_block::block_work () const
{
    return work;
}

void germ::change_block::block_work_set (uint64_t work_a)
{
    work = work_a;
}

germ::block_hash germ::change_block::previous () const
{
    return hashables.previous;
}

void germ::change_block::serialize (germ::stream & stream_a) const
{
    write (stream_a, hashables.previous);
//    write (stream_a, hashables.representative);
    write (stream_a, signature);
    write (stream_a, work);
}

void germ::change_block::serialize_json (std::string & string_a) const
{
    boost::property_tree::ptree tree;
    tree.put ("type", "change");
    tree.put ("previous", hashables.previous.to_string ());
//    tree.put ("representative", representative ().to_account ());
    tree.put ("work", germ::to_string_hex (work));
    std::string signature_l;
    signature.encode_hex (signature_l);
    tree.put ("signature", signature_l);
    std::stringstream ostream;
    boost::property_tree::write_json (ostream, tree);
    string_a = ostream.str ();
}

bool germ::change_block::deserialize (germ::stream & stream_a)
{
    auto error (read (stream_a, hashables.previous));
    if (error)
        return error;

//    error = read (stream_a, hashables.representative);
//    if (error)
//        return error;

    error = read (stream_a, signature);
    if (error)
        return error;

    error = read (stream_a, work);

    return error;
}

bool germ::change_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto error (false);
    try
    {
        assert (tree_a.get<std::string> ("type") == "change");
        auto previous_l (tree_a.get<std::string> ("previous"));
//        auto representative_l (tree_a.get<std::string> ("representative"));
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error = hashables.previous.decode_hex (previous_l);
        if (error)
            return error;

//        error = hashables.representative.decode_hex (representative_l);
//        if (error)
//            return error;

        error = germ::from_string_hex (work_l, work);
        if (error)
            return error;

        error = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
    return error;
}

void germ::change_block::visit (germ::block_visitor & visitor_a) const
{
    visitor_a.change_block (*this);
}

germ::block_type germ::change_block::type () const
{
    return germ::block_type::change;
}

bool germ::change_block::operator== (germ::block const & other_a) const
{
    return blocks_equal (*this, other_a);
}

bool germ::change_block::operator== (germ::change_block const & other_a) const
{
    return hashables.previous == other_a.hashables.previous && /*hashables.representative == other_a.hashables.representative &&*/ work == other_a.work && signature == other_a.signature;
}

bool germ::change_block::valid_predecessor (germ::block const & block_a) const
{
    bool result;
    switch (block_a.type ())
    {
        case germ::block_type::send:
        case germ::block_type::receive:
        case germ::block_type::open:
        case germ::block_type::change:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

germ::block_hash germ::change_block::source () const
{
    return 0;
}

germ::block_hash germ::change_block::root () const
{
    return hashables.previous;
}

germ::account germ::change_block::representative () const
{
//    return hashables.representative;
    return 0;
}

germ::signature germ::change_block::block_signature () const
{
    return signature;
}

void germ::change_block::signature_set (germ::uint512_union const & signature_a)
{
    signature = signature_a;
}

germ::state_hashables::state_hashables (germ::account const & account_a, germ::block_hash const & previous_a, /*germ::account const & representative_a,*/ germ::amount const & balance_a, germ::uint256_union const & link_a) :
account (account_a),
previous (previous_a),
//representative (representative_a),
balance (balance_a),
link (link_a)
{
}

germ::state_hashables::state_hashables (bool & error_a, germ::stream & stream_a)
{
    error_a = germ::read (stream_a, account);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, previous);
    if (error_a)
        return ;

//    error_a = germ::read (stream_a, representative);
//    if (error_a)
//        return ;

    error_a = germ::read (stream_a, balance);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, link);
}

germ::state_hashables::state_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
    try
    {
        auto account_l (tree_a.get<std::string> ("account"));
        auto previous_l (tree_a.get<std::string> ("previous"));
//        auto representative_l (tree_a.get<std::string> ("representative"));
        auto balance_l (tree_a.get<std::string> ("balance"));
        auto link_l (tree_a.get<std::string> ("link"));
        error_a = account.decode_account (account_l);
        if (error_a)
            return ;

        error_a = previous.decode_hex (previous_l);
        if (!error_a)
            return ;

//        error_a = representative.decode_account (representative_l);
//        if (error_a)
//            return ;

        error_a = balance.decode_dec (balance_l);
        if (!error_a)
            return ;

        error_a = link.decode_account (link_l) && link.decode_hex (link_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::state_hashables::hash (blake2b_state & hash_a) const
{
    blake2b_update (&hash_a, account.bytes.data (), sizeof (account.bytes));
    blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
//    blake2b_update (&hash_a, representative.bytes.data (), sizeof (representative.bytes));
    blake2b_update (&hash_a, balance.bytes.data (), sizeof (balance.bytes));
    blake2b_update (&hash_a, link.bytes.data (), sizeof (link.bytes));
}

germ::state_block::state_block (germ::account const & account_a, germ::block_hash const & previous_a, /*germ::account const & representative_a,*/ germ::amount const & balance_a, germ::uint256_union const & link_a, germ::raw_key const & prv_a, germ::public_key const & pub_a, uint64_t work_a) :
hashables (account_a, previous_a, /*representative_a,*/ balance_a, link_a),
signature (germ::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

germ::state_block::state_block (bool & error_a, germ::stream & stream_a) :
hashables (error_a, stream_a)
{
    if (error_a)
        return ;

    error_a = germ::read (stream_a, signature);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, work);
    boost::endian::big_to_native_inplace (work);
}

germ::state_block::state_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
    if (error_a)
        return ;

    try
    {
        auto type_l (tree_a.get<std::string> ("type"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        auto work_l (tree_a.get<std::string> ("work"));
        error_a = type_l != "state";
        if (error_a)
            return ;

        error_a = germ::from_string_hex (work_l, work);
        if (error_a)
            return ;

        error_a = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::state_block::hash (blake2b_state & hash_a) const
{
    germ::uint256_union preamble (static_cast<uint64_t> (germ::block_type::state));
    blake2b_update (&hash_a, preamble.bytes.data (), preamble.bytes.size ());
    hashables.hash (hash_a);
}

uint64_t germ::state_block::block_work () const
{
    return work;
}

void germ::state_block::block_work_set (uint64_t work_a)
{
    work = work_a;
}

germ::block_hash germ::state_block::previous () const
{
    return hashables.previous;
}

void germ::state_block::serialize (germ::stream & stream_a) const
{
    write (stream_a, hashables.account);
    write (stream_a, hashables.previous);
//    write (stream_a, hashables.representative);
    write (stream_a, hashables.balance);
    write (stream_a, hashables.link);
    write (stream_a, signature);
    write (stream_a, boost::endian::native_to_big (work));
}

void germ::state_block::serialize_json (std::string & string_a) const
{
    boost::property_tree::ptree tree;
    tree.put ("type", "state");
    tree.put ("account", hashables.account.to_account ());
    tree.put ("previous", hashables.previous.to_string ());
//    tree.put ("representative", representative ().to_account ());
    tree.put ("balance", hashables.balance.to_string_dec ());
    tree.put ("link", hashables.link.to_string ());
    tree.put ("link_as_account", hashables.link.to_account ());
    std::string signature_l;
    signature.encode_hex (signature_l);
    tree.put ("signature", signature_l);
    tree.put ("work", germ::to_string_hex (work));
    std::stringstream ostream;
    boost::property_tree::write_json (ostream, tree);
    string_a = ostream.str ();
}

bool germ::state_block::deserialize (germ::stream & stream_a)
{
    auto error (read (stream_a, hashables.account));
    if (error)
        return error;

    error = read (stream_a, hashables.previous);
    if (error)
        return error;

//    error = read (stream_a, hashables.representative);
//    if (error)
//        return error;

    error = read (stream_a, hashables.balance);
    if (error)
        return error;

    error = read (stream_a, hashables.link);
    if (error)
        return error;

    error = read (stream_a, signature);
    if (error)
        return error;

    error = read (stream_a, work);
    boost::endian::big_to_native_inplace (work);

    return error;
}

bool germ::state_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto error (false);
    try
    {
        assert (tree_a.get<std::string> ("type") == "state");
        auto account_l (tree_a.get<std::string> ("account"));
        auto previous_l (tree_a.get<std::string> ("previous"));
//        auto representative_l (tree_a.get<std::string> ("representative"));
        auto balance_l (tree_a.get<std::string> ("balance"));
        auto link_l (tree_a.get<std::string> ("link"));
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error = hashables.account.decode_account (account_l);
        if (error)
            return error;

        error = hashables.previous.decode_hex (previous_l);
        if (error)
            return error;

//        error = hashables.representative.decode_account (representative_l);
//        if (error)
//            return error;

        error = hashables.balance.decode_dec (balance_l);
        if (error)
            return error;

        error = hashables.link.decode_account (link_l) && hashables.link.decode_hex (link_l);
        if (error)
            return error;

        error = germ::from_string_hex (work_l, work);
        if (error)
            return error;

        error = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
    return error;
}

void germ::state_block::visit (germ::block_visitor & visitor_a) const
{
    visitor_a.state_block (*this);
}

germ::block_type germ::state_block::type () const
{
    return germ::block_type::state;
}

bool germ::state_block::operator== (germ::block const & other_a) const
{
    return blocks_equal (*this, other_a);
}

bool germ::state_block::operator== (germ::state_block const & other_a) const
{
    return hashables.account == other_a.hashables.account && hashables.previous == other_a.hashables.previous && /*hashables.representative == other_a.hashables.representative &&*/ hashables.balance == other_a.hashables.balance && hashables.link == other_a.hashables.link && signature == other_a.signature && work == other_a.work;
}

bool germ::state_block::valid_predecessor (germ::block const & block_a) const
{
    return true;
}

germ::block_hash germ::state_block::source () const
{
    return 0;
}

germ::block_hash germ::state_block::root () const
{
    return !hashables.previous.is_zero () ? hashables.previous : hashables.account;
}

germ::account germ::state_block::representative () const
{
//    return hashables.representative;
    return 0;
}

germ::signature germ::state_block::block_signature () const
{
    return signature;
}

void germ::state_block::signature_set (germ::uint512_union const & signature_a)
{
    signature = signature_a;
}

std::unique_ptr<germ::tx> germ::deserialize_block_json (boost::property_tree::ptree const & tree_a)
{
    std::unique_ptr<germ::tx> result;
    try
    {
        auto type (tree_a.get<std::string> ("type"));
        if (type == "receive")
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, tree_a));
            if (error)
                return result;

            result = std::move (obj);
        }
        else if (type == "send")
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, tree_a));
            if (error)
                return result;

            result = std::move (obj);
        }
        else if (type == "open")
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, tree_a));
            if (error)
                return result;

            result = std::move (obj);
        }
        else if (type == "change")
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, tree_a));
            if (error)
                return result;

            result = std::move (obj);
        }
        else if (type == "vote")
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, tree_a));
            if (error)
                return result;

            result = std::move (obj);
        }
        else if (type == "state")
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, tree_a));
            if (error)
                return result;

            result = std::move (obj);
        }
    }
    catch (std::runtime_error const &)
    {
    }
    return result;
}

std::unique_ptr<germ::tx> germ::deserialize_block (germ::stream & stream_a)
{
    germ::block_type type;
    auto error (read (stream_a, type));
    std::unique_ptr<germ::tx> result;
    if (error)
        return result;

    size_t body_size;
    error = read(stream_a, body_size);
    if (error)
        return result;

    result = germ::deserialize_block (stream_a, type);
    return result;
}

std::unique_ptr<germ::tx> germ::deserialize_block (germ::stream & stream_a, germ::block_type type_a)
{
    std::unique_ptr<germ::tx> result;
    switch (type_a)
    {
        case germ::block_type::receive:
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, stream_a));
            if (error)
                return result;

            result = std::move (obj);
            break;
        }
        case germ::block_type::send:
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, stream_a));
            if (error)
                return result;

            result = std::move (obj);
            break;
        }
        case germ::block_type::open:
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, stream_a));
            if (error)
                return result;

            result = std::move (obj);
            break;
        }
        case germ::block_type::change:
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, stream_a));
            if (error)
                return result;

            result = std::move (obj);
            break;
        }
        case germ::block_type::vote:
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, stream_a));
            if (error)
                return result;

            result = std::move (obj);
            break;
        }
        case germ::block_type::state:
        {
            bool error;
            std::unique_ptr<germ::tx> obj (new germ::tx (error, stream_a));
            if (error)
                return result;

            result = std::move (obj);
            break;
        }
        default:
            assert (false);
            break;
    }
    return result;
}

void germ::receive_block::visit (germ::block_visitor & visitor_a) const
{
    visitor_a.receive_block (*this);
}

bool germ::receive_block::operator== (germ::receive_block const & other_a) const
{
    auto result (hashables.previous == other_a.hashables.previous && hashables.source == other_a.hashables.source && work == other_a.work && signature == other_a.signature);
    return result;
}

bool germ::receive_block::deserialize (germ::stream & stream_a)
{
    auto error (false);
    error = read (stream_a, hashables.previous.bytes);
    if (error)
        return error;

    error = read (stream_a, hashables.source.bytes);
    if (error)
        return error;

    error = read (stream_a, signature.bytes);
    if (error)
        return error;

    error = read (stream_a, work);
    return error;
}

bool germ::receive_block::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto error (false);
    try
    {
        assert (tree_a.get<std::string> ("type") == "receive");
        auto previous_l (tree_a.get<std::string> ("previous"));
        auto source_l (tree_a.get<std::string> ("source"));
        auto work_l (tree_a.get<std::string> ("work"));
        auto signature_l (tree_a.get<std::string> ("signature"));
        error = hashables.previous.decode_hex (previous_l);
        if (error)
            return error;

        error = hashables.source.decode_hex (source_l);
        if (error)
            return error;

        error = germ::from_string_hex (work_l, work);
        if (error)
            return error;

        error = signature.decode_hex (signature_l);
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
    return error;
}

void germ::receive_block::serialize (germ::stream & stream_a) const
{
    write (stream_a, hashables.previous.bytes);
    write (stream_a, hashables.source.bytes);
    write (stream_a, signature.bytes);
    write (stream_a, work);
}

void germ::receive_block::serialize_json (std::string & string_a) const
{
    boost::property_tree::ptree tree;
    tree.put ("type", "receive");
    std::string previous;
    hashables.previous.encode_hex (previous);
    tree.put ("previous", previous);
    std::string source;
    hashables.source.encode_hex (source);
    tree.put ("source", source);
    std::string signature_l;
    signature.encode_hex (signature_l);
    tree.put ("work", germ::to_string_hex (work));
    tree.put ("signature", signature_l);
    std::stringstream ostream;
    boost::property_tree::write_json (ostream, tree);
    string_a = ostream.str ();
}

germ::receive_block::receive_block (germ::block_hash const & previous_a, germ::block_hash const & source_a, germ::raw_key const & prv_a, germ::public_key const & pub_a, uint64_t work_a) :
hashables (previous_a, source_a),
signature (germ::sign_message (prv_a, pub_a, hash ())),
work (work_a)
{
}

germ::receive_block::receive_block (bool & error_a, germ::stream & stream_a) :
hashables (error_a, stream_a)
{
    if (error_a)
        return ;

    error_a = germ::read (stream_a, signature);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, work);
}

germ::receive_block::receive_block (bool & error_a, boost::property_tree::ptree const & tree_a) :
hashables (error_a, tree_a)
{
    if (error_a)
        return ;

    try
    {
        auto signature_l (tree_a.get<std::string> ("signature"));
        auto work_l (tree_a.get<std::string> ("work"));
        error_a = signature.decode_hex (signature_l);
        if (error_a)
            return ;

        error_a = germ::from_string_hex (work_l, work);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::receive_block::hash (blake2b_state & hash_a) const
{
    hashables.hash (hash_a);
}

uint64_t germ::receive_block::block_work () const
{
    return work;
}

void germ::receive_block::block_work_set (uint64_t work_a)
{
    work = work_a;
}

bool germ::receive_block::operator== (germ::block const & other_a) const
{
    return blocks_equal (*this, other_a);
}

bool germ::receive_block::valid_predecessor (germ::block const & block_a) const
{
    bool result;
    switch (block_a.type ())
    {
        case germ::block_type::send:
        case germ::block_type::receive:
        case germ::block_type::open:
        case germ::block_type::change:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

germ::block_hash germ::receive_block::previous () const
{
    return hashables.previous;
}

germ::block_hash germ::receive_block::source () const
{
    return hashables.source;
}

germ::block_hash germ::receive_block::root () const
{
    return hashables.previous;
}

germ::account germ::receive_block::representative () const
{
    return 0;
}

germ::signature germ::receive_block::block_signature () const
{
    return signature;
}

void germ::receive_block::signature_set (germ::uint512_union const & signature_a)
{
    signature = signature_a;
}

germ::block_type germ::receive_block::type () const
{
    return germ::block_type::receive;
}

germ::receive_hashables::receive_hashables (germ::block_hash const & previous_a, germ::block_hash const & source_a) :
previous (previous_a),
source (source_a)
{
}

germ::receive_hashables::receive_hashables (bool & error_a, germ::stream & stream_a)
{
    error_a = germ::read (stream_a, previous.bytes);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, source.bytes);
}

germ::receive_hashables::receive_hashables (bool & error_a, boost::property_tree::ptree const & tree_a)
{
    try
    {
        auto previous_l (tree_a.get<std::string> ("previous"));
        auto source_l (tree_a.get<std::string> ("source"));
        error_a = previous.decode_hex (previous_l);
        if (error_a)
            return ;

        error_a = source.decode_hex (source_l);
    }
    catch (std::runtime_error const &)
    {
        error_a = true;
    }
}

void germ::receive_hashables::hash (blake2b_state & hash_a) const
{
    blake2b_update (&hash_a, previous.bytes.data (), sizeof (previous.bytes));
    blake2b_update (&hash_a, source.bytes.data (), sizeof (source.bytes));
}
