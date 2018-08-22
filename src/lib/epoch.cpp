//
// Created by daoful on 18-7-23.
//

#include <src/lib/epoch.h>
#include <src/epochstore.h>
#include <src/ledger.hpp>

/* ------------------------------------------------------ Constructors ------------------------------------------------------------------*/
// default constructor
germ::epoch::epoch()
:timestamp_(0)
{
    prev_.clear();
    signature_.clear();
}

// constructor to set all member variables with input parameters
germ::epoch::epoch(uint64_t timestamp_r,
                   germ::epoch_hash const & previous_r,
                   germ::signature const & signature_r,
                   std::vector<germ::block_hash> txs_r,
                   std::vector<germ::signature> pre_votes_r,
                   std::vector<germ::signature> votes_r)
:timestamp_(timestamp_r),
 prev_(previous_r),
 signature_(signature_r),
 txs_(txs_r),
 pre_votes_(pre_votes_r),
 votes_(votes_r)
{
}

// constructor using stream as input
germ::epoch::epoch(bool & error, germ::stream & stream_r)
{
    error = deserialize(stream_r);
}

// constructor using propery_tree as input
germ::epoch::epoch(bool & error, boost::property_tree::ptree const & tree_r)
{
    error = deserialize_json(tree_r);
}

//germ::epoch::epoch (MDB_val const & val_a)
//{
//    assert (val_a.mv_size == sizeof (*this));
//    static_assert (size() == sizeof (*this), "Packed class");
//    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
//}

/* operations */
germ::epoch_hash germ::epoch::hash() const
{
    germ::uint256_union result;
    blake2b_state hash_;
    auto status (blake2b_init (&hash_, sizeof (result.bytes)));
    assert (status == 0);

    hash(hash_);

    status = blake2b_final (&hash_, result.bytes.data (), sizeof (result.bytes));
    assert (status == 0);
    return result;
}

std::string germ::epoch::to_json()
{
    std::string result;
    serialize_json(result);

    return result;
}

void germ::epoch::hash(blake2b_state &hash_r) const
{
    auto status (blake2b_update (&hash_r, prev_.bytes.data (), sizeof (prev_.bytes)));
    assert (status == 0);

    status = blake2b_update (&hash_r, signature_.bytes.data (), sizeof (signature_.bytes));
    assert (status == 0);

    for(auto tx:txs_)
    {
        status = blake2b_update (&hash_r, tx.bytes.data (), sizeof (tx.bytes));
        assert(status ==0);
    }

    for(auto pre_vote:pre_votes_)
    {
        status = blake2b_update (&hash_r, pre_vote.bytes.data (), sizeof (pre_vote.bytes));
        assert(status ==0);
    }

    for(auto vote:votes_)
    {
        status = blake2b_update (&hash_r, vote.bytes.data (), sizeof (vote.bytes));
        assert(status ==0);
    }
}

germ::epoch_hash germ::epoch::previous() const
{
    return prev_;
}


void germ::epoch::serialize(germ::stream & stream_r) const
{
    write (stream_r, timestamp_);
    write (stream_r, prev_.bytes);

    uint32_t txs_len = (uint32_t) txs_.size();
    write(stream_r,txs_len);
    for(germ::epoch_hash i : txs_)
        write (stream_r, i.bytes);

    uint32_t pre_votes_len = (uint32_t) pre_votes_.size();
    write(stream_r,pre_votes_len);
    for(germ::signature i : pre_votes_)
        write (stream_r, i.bytes);

    uint32_t votes_len = (uint32_t) pre_votes_.size();
    write(stream_r, votes_len);
    for(germ::signature i : votes_)
        write (stream_r, i.bytes);

    write (stream_r, signature_.bytes);
}

bool germ::epoch::deserialize(germ::stream &stream_r)
{
    auto error(false);

    error = germ::read(stream_r, timestamp_);
    if (error)
        return error;

    error = germ::read(stream_r, prev_.bytes);
    if (error)
        return error;

    // read in txs (length first, then elements)
    uint32_t txs_len;
    error = germ::read(stream_r, txs_len);
    if (error)
        return error;

    germ::epoch_hash tx_hash_holder;
    for(uint32_t i=0; i<txs_len; i++)
    {
        error = germ::read (stream_r, tx_hash_holder);
        if (error)
            return error;
        else
            txs_.push_back(tx_hash_holder);
    }

    // read in pre-votes (length first, then elements)
    uint32_t pre_votes_len;
    error = germ::read(stream_r, pre_votes_len);
    if (error)
        return error;

    germ::signature pre_vote_holder;
    for(uint32_t i=0; i<pre_votes_len; i++)
    {
        error = germ::read (stream_r, pre_vote_holder);
        if (error)
            return error;
        else
            pre_votes_.push_back(pre_vote_holder);
    }


    // read in votes (length first, then elements)
    uint32_t votes_len;
    error = germ::read(stream_r, votes_len);
    if (error)
        return error;

    germ::signature vote_holder;
    for(uint32_t i=0; i<votes_len; i++)
    {
        error = germ::read (stream_r, vote_holder);
        if (error)
            return error;
        else
            pre_votes_.push_back(vote_holder);
    }

    error = germ::read(stream_r, signature_.bytes);

    return error;
}

void germ::epoch::serialize_json(std::string &string) const
{
    boost::property_tree::ptree tree;
    tree.put("type", "epoch");

    std::string time(germ::to_string_hex(timestamp_));
    tree.put("timestamp", time);

    std::string previous_r;
    prev_.encode_hex(previous_r);
    tree.put("previous", previous_r);

    // fill the "txs" sub-tree, and put it in tree
    boost::property_tree::ptree txs_r;
    for(germ::epoch_hash i : txs_)
        txs_r.put(i.to_string(),"");
    tree.put_child("txs",txs_r);

    // fill the "pre_votes" sub-tree, and put it in tree
    boost::property_tree::ptree pre_votes_r;
    for(germ::signature i : pre_votes_)
        pre_votes_r.put(i.to_string(),"");
    tree.put_child("pre_votes",pre_votes_r);

    // fill the "votes" sub-tree, and put it in tree
    boost::property_tree::ptree votes_r;
    for(germ::signature i : votes_)
        votes_r.put(i.to_string(),"");
    tree.put_child("votes",votes_r);

    std::string signature_r;
    signature_.encode_hex(signature_r);
    tree.put("signature", signature_r);

    std::stringstream ostream;
    boost::property_tree::write_json(ostream, tree);
    string = ostream.str();
}

bool germ::epoch::deserialize_json(boost::property_tree::ptree const &tree_r)
{
    auto error(false);
    try
    {
        assert(tree_r.get<std::string>("type") == "epoch");
        auto timestamp_r(tree_r.get<std::string>("timestamp"));
        auto previous_r(tree_r.get<std::string>("previous"));

        boost::property_tree::ptree txs_r(tree_r.get_child("txs"));
        boost::property_tree::ptree pre_votes_r(tree_r.get_child("pre_votes"));
        boost::property_tree::ptree votes_r(tree_r.get_child("votes"));

        auto signature_r(tree_r.get<std::string>("signature"));

        error = germ::from_string_hex(timestamp_r, timestamp_);
        if (error)
            return error;

        error = prev_.decode_hex(previous_r);
        if (error)
            return error;

        for (auto i (txs_r.begin ()), n (txs_r.end ()); i != n; ++i)
            txs_.push_back(germ::block_hash(i->first));


        for (auto i (pre_votes_r.begin ()), n (pre_votes_r.end ()); i != n; ++i)
            pre_votes_.push_back(germ::signature(i->first));


        for (auto i (votes_r.begin ()), n (votes_r.end ()); i != n; ++i)
            votes_.push_back(germ::signature(i->first));

        error = signature_.decode_hex(signature_r);
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }

    return error;
}

germ::block_hash germ::epoch::previous_epoch() const
{
    return germ::block_hash(0);
}

bool germ::epoch::operator==(germ::epoch const &other_r) const
{
    auto result( timestamp_ == other_r.timestamp_ && prev_ == other_r.prev_ &&
                 signature_ == other_r.signature_ && txs_ == other_r.txs_ && pre_votes_ == other_r.pre_votes_ &&
                 votes_ == other_r.votes_);
    return result;
}

bool germ::epoch::operator==(germ::block const &other_r) const
{
    return false;
}

germ::block_type germ::epoch::type() const
{
    return germ::block_type ::epoch;
}

void germ::epoch::visit(germ::epoch_visitor & visit_r) const
{
    visit_r.epoch_block(*this);
}

void germ::epoch::visit(germ::block_visitor & visit_r) const
{
    
}


/* Accessor methods */
uint64_t germ::epoch::timestamp() const
{
    return timestamp_;
}

germ::epoch_hash germ::epoch::prev() const
{
    return prev_;
}


std::vector<germ::block_hash> germ::epoch::txs() const
{
    return txs_;
}

std::vector<germ::signature> germ::epoch::pre_votes() const
{
    return pre_votes_;
}

std::vector<germ::signature> germ::epoch::votes() const
{
    return votes_;
}

germ::signature germ::epoch::signature() const
{
    return signature_;
}

/* Mutator methods*/
void germ::epoch::set_timestamp(uint64_t const & timestamp_r)
{
    timestamp_ = timestamp_r;
}

void germ::epoch::set_prev(germ::epoch_hash const & prev_r)
{
    prev_ = prev_r;
}

void germ::epoch::set_txs(std::vector<germ::block_hash> const & txs_r)
{
    txs_ = txs_r;
}

void germ::epoch::set_pre_votes(std::vector<germ::signature> const & pre_votes_r)
{
    pre_votes_ = pre_votes_r;
}

void germ::epoch::set_votes(std::vector<germ::signature> const & votes_r)
{
    votes_ = votes_r;
}

void germ::epoch::set_signature(germ::uint512_union const &signature_r)
{
    signature_ = signature_r;
}

bool germ::epoch::valid_predecessor(germ::epoch const & epoch_r) const
{
    bool result;
    switch (epoch_r.type ())
    {
        case germ::block_type::epoch:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

//static constexpr size_t germ::epoch::size(germ::epoch const & epoch_r)
//{
//    size_t s;
//    s = sizeof (uint64_t)   + sizeof (germ::epoch_hash)  + sizeof (epoch_hash) + sizeof (germ::signature);
//    s+= (sizeof (germ::block_hash) * epoch_r.txs_.size() +
//         sizeof (germ::signature) * (epoch_r.pre_votes_.size() + epoch_r.votes_.size()));
//    return s;
//}

size_t germ::epoch::size() const
{
    size_t s;
    s = sizeof (uint64_t)   + sizeof (germ::epoch_hash)  + sizeof (germ::signature);
    s+= (sizeof (germ::block_hash) * txs_.size() +
         sizeof (germ::signature) * (pre_votes_.size() + votes_.size()));
    return s;
}

//germ::mdb_val germ::epoch::val () const
//{
//    return germ::mdb_val (sizeof (*this), const_cast<germ::epoch*> (this));
//}

germ::epoch_message::epoch_message()
:epoch_hash_(0)
{
}

germ::epoch_message::epoch_message(germ::epoch_hash const & epoch_r)
{
    epoch_hash_ = epoch_r;
}

germ::epoch_message::epoch_message(uint64_t timestamp_r, germ::epoch_hash const & previous_r,
                                   germ::signature const & signature_r,
                                   germ::epoch_hash const & epoch_hash_r, std::vector<germ::block_hash> txs_r,
                                   std::vector<germ::signature> pre_votes_r, std::vector<germ::signature> votes_r)
{
    this->set_timestamp(timestamp_r);   // = timestamp_r;
    this->set_prev(previous_r);         //= previous_r;
    this->set_signature(signature_r);   // = signature_r;
    this->set_txs(txs_r);
    this->set_pre_votes(pre_votes_r);
    this->set_votes(votes_r);

    this->epoch_hash_ = epoch_hash_r;
}

germ::epoch_hash germ::epoch_message::epoch_hash() const
{
    return epoch_hash_;
}
void germ::epoch_message::set_epoch_hash(uint64_t const & epoch_hash_r)
{
    epoch_hash_ = epoch_hash_r;
}

std::unique_ptr<germ::epoch> germ::deserialize_epoch (germ::stream & stream_r)
{
    germ::block_type type;
    auto error (read (stream_r, type));
    std::unique_ptr<germ::epoch> result;
    if (error)
        return result;

    result = germ::deserialize_epoch (stream_r, type);
    return result;
}

std::unique_ptr<germ::epoch> germ::deserialize_epoch (germ::stream & stream_r, germ::block_type type_r)
{
    std::unique_ptr<germ::epoch> result;
    switch (type_r)
    {
        case germ::block_type::epoch:
        {
            bool error;
            std::unique_ptr<germ::epoch> obj (new germ::epoch (error, stream_r));
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

std::unique_ptr<germ::epoch> germ::deserialize_epoch_json (boost::property_tree::ptree const & tree_r)
{
    std::unique_ptr<germ::epoch> result;
    try
    {
        auto type (tree_r.get<std::string> ("type"));
        if (type == "epoch")
        {
            bool error;
            std::unique_ptr<germ::epoch> obj (new germ::epoch (error, tree_r));
            if (error)
                return result;

            result = std::move (obj);
        }
        else
        {

        }
    }
    catch (std::runtime_error const &)
    {
    }
    return result;
}


// Serialize a block prefixed with an 8-bit typecode
void germ::serialize_epoch (germ::stream & stream_r, germ::epoch const & epoch_r)
{
    write (stream_r, epoch_r.type ());
    write (stream_r, epoch_r.size ());
    epoch_r.serialize (stream_r);
}
