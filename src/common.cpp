#include <src/common.hpp>

#include <src/blockstore.hpp>
#include <src/lib/interface.h>
#include <src/node/common.hpp>
#include <src/versioning.hpp>

#include <boost/property_tree/json_parser.hpp>

#include <queue>

#include <ed25519-donna/ed25519.h>
#include <src/lib/tx.h>

// Genesis keys for network variants
namespace
{
char const * test_private_key_data = "34F0A37AAD20F4A260F0A5B3CB3D7FB50673212263E58A380BC10474BB039CE4";
char const * test_public_key_data = "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0"; // xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo
char const * beta_public_key_data = "A59A47CC4F593E75AE9AD653FDA9358E2F7898D9ACC8C60E80D0495CE20FBA9F"; // xrb_3betaz86ypbygpqbookmzpnmd5jhh4efmd8arr9a3n4bdmj1zgnzad7xpmfp
char const * live_public_key_data = "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA"; // xrb_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3
char const * test_genesis_data = R"%%%({
    "type": "receive",
    "previous": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
    "destination": "",
    "balance":"0",
    "source": "B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0",
    "representative": "xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "account": "xrb_3e3j5tkog48pnny9dmfzj1r16pg8t1e76dz5tmac6iq689wyjfpiij4txtdo",
    "tx_info": {
        "value":"0x10",
        "data":"0x88888880988888888888888888888888888888099999999877",
        "gas":"32000",
        "gasprice":"100"
    },
    "epoch":"0000000000000000000000000000000000000000000000000000000000000000",

    "signature": "8544566105676193C33E8B95169EF96CCC44AE26A1652B5B236489A764187A328D813FEA8D2EE3D9229E96AB411FFDB27DEE5FE74EC223A65EC524A88900160F"
})%%%";

// ECDA914373A2F0CA1296475BAEE40500A7F0A7AD72A5A80C81D7FAB7F6C802B2CC7DB50F5DD0FB25B2EF11761FA7344A158DD5A700B21BD47DE5BD0F63153A02
// 8544566105676193C33E8B95169EF96CCC44AE26A1652B5B236489A764187A328D813FEA8D2EE3D9229E96AB411FFDB27DEE5FE74EC223A65EC524A88900160F
char const * beta_genesis_data = R"%%%({
        "type": "open",
        "source": "A59A47CC4F593E75AE9AD653FDA9358E2F7898D9ACC8C60E80D0495CE20FBA9F",
        "representative": "xrb_3betaz86ypbygpqbookmzpnmd5jhh4efmd8arr9a3n4bdmj1zgnzad7xpmfp",
        "account": "xrb_3betaz86ypbygpqbookmzpnmd5jhh4efmd8arr9a3n4bdmj1zgnzad7xpmfp",
        "work": "dc4914cce98187b5",
        "signature": "A726490E3325E4FA59C1C900D5B6EEBB15FE13D99F49D475B93F0AACC5635929A0614CF3892764A04D1C6732A0D716FFEB254D4154C6F544D11E6630F201450B"
})%%%";

char const * live_genesis_data = R"%%%({
    "type": "open",
    "source": "E89208DD038FBB269987689621D52292AE9C35941A7484756ECCED92A65093BA",
    "representative": "xrb_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3",
    "account": "xrb_3t6k35gi95xu6tergt6p69ck76ogmitsa8mnijtpxm9fkcm736xtoncuohr3",
    "work": "62f05417dd3fb691",
    "signature": "9F0C933C8ADE004D808EA1985FA746A7E95BA2A38F867640F53EC8F180BDFE9E2C1268DEAD7C2664F356E37ABA362BC58E46DBA03E523A7B5A19E4B6EB12BB02"
})%%%";

class ledger_constants
{
public:
    ledger_constants () :
    zero_key ("0"),
    test_genesis_key (test_private_key_data),
    rai_test_account (test_public_key_data),
    rai_beta_account (beta_public_key_data),
    rai_live_account (live_public_key_data),
    rai_test_genesis (test_genesis_data),
    rai_beta_genesis (beta_genesis_data),
    rai_live_genesis (live_genesis_data),
    genesis_account (germ::rai_network == germ::germ_networks::germ_test_network ? rai_test_account : germ::rai_network == germ::germ_networks::germ_beta_network ? rai_beta_account : rai_live_account),
    genesis_block (germ::rai_network == germ::germ_networks::germ_test_network ? rai_test_genesis : germ::rai_network == germ::germ_networks::germ_beta_network ? rai_beta_genesis : rai_live_genesis),
    genesis_amount (std::numeric_limits<germ::uint128_t>::max ()),
    burn_account (0)
    {
        CryptoPP::AutoSeededRandomPool random_pool;
        // Randomly generating these mean no two nodes will ever have the same sentinel values which protects against some insecure algorithms
        random_pool.GenerateBlock (not_a_block.bytes.data (), not_a_block.bytes.size ());
        random_pool.GenerateBlock (not_an_account.bytes.data (), not_an_account.bytes.size ());
    }
    germ::keypair zero_key;
    germ::keypair test_genesis_key;
    germ::account rai_test_account;
    germ::account rai_beta_account;
    germ::account rai_live_account;
    std::string rai_test_genesis;
    std::string rai_beta_genesis;
    std::string rai_live_genesis;
    germ::account genesis_account;
    std::string genesis_block;
    germ::uint128_t genesis_amount;
    germ::block_hash not_a_block;
    germ::account not_an_account;
    germ::account burn_account;
};
ledger_constants globals;
}

size_t constexpr germ::send_block::size;
size_t constexpr germ::receive_block::size;
size_t constexpr germ::open_block::size;
size_t constexpr germ::change_block::size;
size_t constexpr germ::state_block::size;

germ::keypair const & germ::zero_key (globals.zero_key);
germ::keypair const & germ::test_genesis_key (globals.test_genesis_key);
germ::account const & germ::rai_test_account (globals.rai_test_account);
germ::account const & germ::rai_beta_account (globals.rai_beta_account);
germ::account const & germ::rai_live_account (globals.rai_live_account);
std::string const & germ::rai_test_genesis (globals.rai_test_genesis);
std::string const & germ::rai_beta_genesis (globals.rai_beta_genesis);
std::string const & germ::rai_live_genesis (globals.rai_live_genesis);

germ::account const & germ::genesis_account (globals.genesis_account);
std::string const & germ::genesis_block (globals.genesis_block);
germ::uint128_t const & germ::genesis_amount (globals.genesis_amount);
germ::block_hash const & germ::not_a_block (globals.not_a_block);
germ::block_hash const & germ::not_an_account (globals.not_an_account);
germ::account const & germ::burn_account (globals.burn_account);

germ::votes::votes (std::shared_ptr<germ::tx> block_a) :
id (block_a->previous())
//id (block_a->root ())
{
    rep_votes.insert (std::make_pair (germ::not_an_account, block_a));
}

germ::tally_result germ::votes::vote (std::shared_ptr<germ::vote> vote_a)
{
    germ::tally_result result;
    auto existing (rep_votes.find (vote_a->account));
    if (existing == rep_votes.end ())
    {
        // Vote on this block hasn't been seen from rep before
        result = germ::tally_result::vote;
        rep_votes.insert (std::make_pair (vote_a->account, vote_a->block));
    }
    else
    {
        if (!(*existing->second == *vote_a->block))
        {
            // Rep changed their vote
            result = germ::tally_result::changed;
            existing->second = vote_a->block;
        }
        else
        {
            // Rep vote remained the same
            result = germ::tally_result::confirm;
        }
    }
    return result;
}

bool germ::votes::uncontested ()
{
    bool result (true);
    if (!rep_votes.empty ())
    {
        auto block (rep_votes.begin ()->second);
        for (auto i (rep_votes.begin ()), n (rep_votes.end ()); result && i != n; ++i)
        {
            result = *i->second == *block;
        }
    }
    return result;
}

// Create a new random keypair
germ::keypair::keypair ()
{
    random_pool.GenerateBlock (prv.data.bytes.data (), prv.data.bytes.size ());
    ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a private key
germ::keypair::keypair (germ::raw_key && prv_a) :
prv (std::move (prv_a))
{
    ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Create a keypair given a hex string of the private key
germ::keypair::keypair (std::string const & prv_a)
{
    auto error (prv.data.decode_hex (prv_a));
    assert (!error);
    ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
}

// Serialize a block prefixed with an 8-bit typecode
void germ::serialize_block (germ::stream & stream_a, germ::tx const & block_a)
{
    write (stream_a, block_a.type ());
    write (stream_a, block_a.size ());
    block_a.serialize (stream_a);
}

std::unique_ptr<germ::tx> germ::deserialize_block (MDB_val const & val_a)
{
    germ::bufferstream stream (reinterpret_cast<uint8_t const *> (val_a.mv_data), val_a.mv_size);
    return deserialize_block (stream);
}

germ::account_info::account_info () :
head (0),
//rep_block (0),
open_block (0),
balance (0),
modified (0),
block_count (0)
{
}

germ::account_info::account_info (MDB_val const & val_a)
{
    assert (val_a.mv_size == sizeof (*this));
    static_assert (sizeof (head) + /*sizeof (rep_block) +*/ sizeof (open_block) + sizeof (balance) + sizeof (modified) + sizeof (block_count) == sizeof (*this), "Class not packed");
    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

germ::account_info::account_info (germ::block_hash const & head_a, /*germ::block_hash const & rep_block_a,*/ germ::block_hash const & open_block_a, germ::amount const & balance_a, uint64_t modified_a, uint64_t block_count_a) :
head (head_a),
//rep_block (rep_block_a),
open_block (open_block_a),
balance (balance_a),
modified (modified_a),
block_count (block_count_a)
{
}

void germ::account_info::serialize (germ::stream & stream_a) const
{
    write (stream_a, head.bytes);
//    write (stream_a, rep_block.bytes);
    write (stream_a, open_block.bytes);
    write (stream_a, balance.bytes);
    write (stream_a, modified);
    write (stream_a, block_count);
}

bool germ::account_info::deserialize (germ::stream & stream_a)
{
    auto error (read (stream_a, head.bytes));
    if (error)
        return error;

//    error = read (stream_a, rep_block.bytes);
//    if (error)
//        return error;

    error = read (stream_a, open_block.bytes);
    if (error)
        return error;

    error = read (stream_a, balance.bytes);
    if (error)
        return error;

    error = read (stream_a, modified);
    if (error)
        return error;

    error = read (stream_a, block_count);
    return error;
}

bool germ::account_info::operator== (germ::account_info const & other_a) const
{
    return head == other_a.head /*&& rep_block == other_a.rep_block*/ && open_block == other_a.open_block && balance == other_a.balance && modified == other_a.modified && block_count == other_a.block_count;
}

bool germ::account_info::operator!= (germ::account_info const & other_a) const
{
    return !(*this == other_a);
}

germ::mdb_val germ::account_info::val () const
{
    return germ::mdb_val (sizeof (*this), const_cast<germ::account_info *> (this));
}

germ::block_counts::block_counts () :
send (0),
receive (0),
open (0),
change (0),
state (0)
{
}

size_t germ::block_counts::sum ()
{
    return send + receive + open + change + state;
}

germ::pending_info::pending_info () :
source (0),
amount (0)
{
}

germ::pending_info::pending_info (MDB_val const & val_a)
{
    assert (val_a.mv_size == sizeof (*this));
    static_assert (sizeof (source) + sizeof (amount) == sizeof (*this), "Packed class");
    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

germ::pending_info::pending_info (germ::account const & source_a, germ::amount const & amount_a) :
source (source_a),
amount (amount_a)
{
}

void germ::pending_info::serialize (germ::stream & stream_a) const
{
    germ::write (stream_a, source.bytes);
    germ::write (stream_a, amount.bytes);
}

bool germ::pending_info::deserialize (germ::stream & stream_a)
{
    auto result (germ::read (stream_a, source.bytes));
    if (!result)
    {
        result = germ::read (stream_a, amount.bytes);
    }
    return result;
}

bool germ::pending_info::operator== (germ::pending_info const & other_a) const
{
    return source == other_a.source && amount == other_a.amount;
}

germ::mdb_val germ::pending_info::val () const
{
    return germ::mdb_val (sizeof (*this), const_cast<germ::pending_info *> (this));
}

germ::pending_key::pending_key (germ::account const & account_a, germ::block_hash const & hash_a) :
account (account_a),
hash (hash_a)
{
}

germ::pending_key::pending_key (MDB_val const & val_a)
{
    assert (val_a.mv_size == sizeof (*this));
    static_assert (sizeof (account) + sizeof (hash) == sizeof (*this), "Packed class");
    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

void germ::pending_key::serialize (germ::stream & stream_a) const
{
    germ::write (stream_a, account.bytes);
    germ::write (stream_a, hash.bytes);
}

bool germ::pending_key::deserialize (germ::stream & stream_a)
{
    auto error (germ::read (stream_a, account.bytes));
    if (!error)
    {
        error = germ::read (stream_a, hash.bytes);
    }
    return error;
}

bool germ::pending_key::operator== (germ::pending_key const & other_a) const
{
    return account == other_a.account && hash == other_a.hash;
}

germ::mdb_val germ::pending_key::val () const
{
    return germ::mdb_val (sizeof (*this), const_cast<germ::pending_key *> (this));
}

germ::block_info::block_info () :
account (0),
balance (0)
{
}

germ::block_info::block_info (MDB_val const & val_a)
{
    assert (val_a.mv_size == sizeof (*this));
    static_assert (sizeof (account) + sizeof (balance) == sizeof (*this), "Packed class");
    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

germ::block_info::block_info (germ::account const & account_a, germ::amount const & balance_a) :
account (account_a),
balance (balance_a)
{
}

void germ::block_info::serialize (germ::stream & stream_a) const
{
    germ::write (stream_a, account.bytes);
    germ::write (stream_a, balance.bytes);
}

bool germ::block_info::deserialize (germ::stream & stream_a)
{
    auto error (germ::read (stream_a, account.bytes));
    if (!error)
    {
        error = germ::read (stream_a, balance.bytes);
    }
    return error;
}

bool germ::block_info::operator== (germ::block_info const & other_a) const
{
    return account == other_a.account && balance == other_a.balance;
}

germ::mdb_val germ::block_info::val () const
{
    return germ::mdb_val (sizeof (*this), const_cast<germ::block_info *> (this));
}

germ::epoch_info::epoch_info () :
head (0),
modified (0),
block_count (0)
{
}

germ::epoch_info::epoch_info (MDB_val const & val_a)
{
    assert (val_a.mv_size == sizeof (*this));
    static_assert (sizeof (head) + sizeof (modified) + sizeof (block_count) == sizeof (*this), "Class not packed");
    std::copy (reinterpret_cast<uint8_t const *> (val_a.mv_data), reinterpret_cast<uint8_t const *> (val_a.mv_data) + sizeof (*this), reinterpret_cast<uint8_t *> (this));
}

germ::epoch_info::epoch_info (germ::block_hash const & head_a, uint64_t modified_a, uint64_t block_count_a) :
head (head_a),
modified (modified_a),
block_count (block_count_a)
{
}

void germ::epoch_info::serialize (germ::stream & stream_a) const
{
    write (stream_a, head.bytes);
    write (stream_a, modified);
    write (stream_a, block_count);
}

bool germ::epoch_info::deserialize (germ::stream & stream_a)
{
    auto error (read (stream_a, head.bytes));
    if (error)
        return error;

    error = read (stream_a, modified);
    if (error)
        return error;

    error = read (stream_a, block_count);
    return error;
}

bool germ::epoch_info::operator== (germ::epoch_info const & other_a) const
{
    return head == other_a.head && modified == other_a.modified && block_count == other_a.block_count;
}

bool germ::epoch_info::operator!= (germ::epoch_info const & other_a) const
{
    return !(*this == other_a);
}

germ::mdb_val germ::epoch_info::val () const
{
    return germ::mdb_val (sizeof (*this), const_cast<germ::epoch_info *> (this));
}


bool germ::vote::operator== (germ::vote const & other_a) const
{
    return sequence == other_a.sequence && *block == *other_a.block && account == other_a.account && signature == other_a.signature;
}

bool germ::vote::operator!= (germ::vote const & other_a) const
{
    return !(*this == other_a);
}

std::string germ::vote::to_json () const
{
    std::stringstream stream;
    boost::property_tree::ptree tree;
    tree.put ("account", account.to_account ());
    tree.put ("signature", signature.number ());
    tree.put ("sequence", std::to_string (sequence));
    tree.put ("block", block->to_json ());
    boost::property_tree::write_json (stream, tree);
    return stream.str ();
}

germ::amount_visitor::amount_visitor (MDB_txn * transaction_a, germ::block_store & store_a) :
transaction (transaction_a),
store (store_a),
current_amount (0),
current_balance (0),
amount (0)
{
}

void germ::amount_visitor::send_block (germ::send_block const & block_a)
{
    current_balance = block_a.hashables.previous;
    amount = block_a.hashables.balance.number ();
    current_amount = 0;
}

void germ::amount_visitor::receive_block (germ::receive_block const & block_a)
{
    current_amount = block_a.hashables.source;
}

void germ::amount_visitor::open_block (germ::open_block const & block_a)
{
    if (block_a.hashables.source != germ::genesis_account)
    {
        current_amount = block_a.hashables.source;
    }
    else
    {
        amount = germ::genesis_amount;
        current_amount = 0;
    }
}

void germ::amount_visitor::state_block (germ::state_block const & block_a)
{
    current_balance = block_a.hashables.previous;
    amount = block_a.hashables.balance.number ();
    current_amount = 0;
}

void germ::amount_visitor::change_block (germ::change_block const & block_a)
{
    amount = 0;
    current_amount = 0;
}

void germ::amount_visitor::compute (germ::block_hash const & block_hash)
{
    current_amount = block_hash;
    while (!current_amount.is_zero () || !current_balance.is_zero ())
    {
        if (!current_amount.is_zero ())
        {
            auto block (store.block_get (transaction, current_amount));
            if (block != nullptr)
            {
                block->visit (*this);
            }
            else
            {
                if (block_hash == germ::genesis_account)
                {
                    amount = std::numeric_limits<germ::uint128_t>::max ();
                    current_amount = 0;
                }
                else
                {
                    assert (false);
                    amount = 0;
                    current_amount = 0;
                }
            }
        }
        else
        {
            balance_visitor prev (transaction, store);
            prev.compute (current_balance);
            amount = amount < prev.balance ? prev.balance - amount : amount - prev.balance;
            current_balance = 0;
        }
    }
}

void germ::amount_visitor::tx(germ::tx const& tx)
{
    germ::block_type tx_type = tx.type();
    if (tx_type == germ::block_type::send || tx_type == germ::block_type::vote)
    {
//        germ::send_tx block = dynamic_cast<germ::send_tx const &>(block_r);
//        current_balance = block.hashables.previous;
//        amount = block.hashables.balance.number ();
//        current_amount = 0;
    }
    else if (tx_type == germ::block_type::receive)
    {
//        current_amount = dynamic_cast<germ::receive_tx const &>(block_r).hashables.source;
    }
}

germ::balance_visitor::balance_visitor (MDB_txn * transaction_a, germ::block_store & store_a) :
transaction (transaction_a),
store (store_a),
current_balance (0),
current_amount (0),
balance (0)
{
}

void germ::balance_visitor::send_block (germ::send_block const & block_a)
{
    balance += block_a.hashables.balance.number ();
    current_balance = 0;
}

void germ::balance_visitor::receive_block (germ::receive_block const & block_a)
{
    germ::block_info block_info;
    if (store.block_info_get (transaction, block_a.hash (), block_info))
    {
        balance += block_info.balance.number ();
        current_balance = 0;
    }
    else
    {
        current_amount = block_a.hashables.source;
        current_balance = block_a.hashables.previous;
    }
}

void germ::balance_visitor::open_block (germ::open_block const & block_a)
{
    current_amount = block_a.hashables.source;
    current_balance = 0;
}

void germ::balance_visitor::change_block (germ::change_block const & block_a)
{
    germ::block_info block_info;
    if (store.block_info_get (transaction, block_a.hash (), block_info))
    {
        balance += block_info.balance.number ();
        current_balance = 0;
    }
    else
    {
        current_balance = block_a.hashables.previous;
    }
}

void germ::balance_visitor::state_block (germ::state_block const & block_a)
{
    balance = block_a.hashables.balance.number ();
    current_balance = 0;
}

void germ::balance_visitor::compute (germ::block_hash const & block_hash)
{
    current_balance = block_hash;
    while (!current_balance.is_zero () || !current_amount.is_zero ())
    {
        if (!current_amount.is_zero ())
        {
            amount_visitor source (transaction, store);
            source.compute (current_amount);
            balance += source.amount;
            current_amount = 0;
        }
        else
        {
            auto block (store.block_get (transaction, current_balance));
            assert (block != nullptr);
            block->visit (*this);
        }
    }
}

void germ::balance_visitor::tx(germ::tx const& tx)
{
    germ::block_type tx_type = tx.type();
    if (tx_type == germ::block_type::send || tx_type == germ::block_type::vote)
    {
        balance += tx.balance_.number ();
        current_balance = 0;
    }
    else if(tx_type == germ::block_type::receive)
    {
        germ::block_info block_info;
        if (store.block_info_get (transaction, tx.hash (), block_info))
        {
            balance += block_info.balance.number ();
            current_balance = 0;
        }
        else
        {
            current_amount = tx.source_;
            current_balance = tx.previous_;
        }
    }
}

germ::representative_visitor::representative_visitor (MDB_txn * transaction_a, germ::block_store & store_a) :
transaction (transaction_a),
store (store_a),
result (0)
{
}

void germ::representative_visitor::compute (germ::block_hash const & hash_a)
{
    current = hash_a;
    while (result.is_zero ())
    {
        auto block (store.block_get (transaction, current));
        assert (block != nullptr);
        block->visit (*this);
    }
}

void germ::representative_visitor::send_block (germ::send_block const & block_a)
{
    current = block_a.previous ();
}

void germ::representative_visitor::receive_block (germ::receive_block const & block_a)
{
    current = block_a.previous ();
}

void germ::representative_visitor::open_block (germ::open_block const & block_a)
{
    result = block_a.hash ();
}

void germ::representative_visitor::change_block (germ::change_block const & block_a)
{
    result = block_a.hash ();
}

void germ::representative_visitor::state_block (germ::state_block const & block_a)
{
    result = block_a.hash ();
}

void germ::representative_visitor::tx(germ::tx const& tx)
{
    current = tx.previous();
}

germ::vote::vote (germ::vote const & other_a) :
sequence (other_a.sequence),
block (other_a.block),
account (other_a.account),
signature (other_a.signature)
{
}

germ::vote::vote (bool & error_a, germ::stream & stream_a)
{
    if (error_a)
        return ;

    error_a = germ::read (stream_a, account.bytes);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, signature.bytes);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, sequence);
    if (error_a)
        return ;

    block = germ::deserialize_block (stream_a);
    error_a = block == nullptr;
}

germ::vote::vote (bool & error_a, germ::stream & stream_a, germ::block_type type_a)
{
    if (error_a)
        return ;

    error_a = germ::read (stream_a, account.bytes);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, signature.bytes);
    if (error_a)
        return ;

    error_a = germ::read (stream_a, sequence);
    if (error_a)
        return ;

    block = germ::deserialize_block (stream_a, type_a);
    error_a = block == nullptr;
}

germ::vote::vote (germ::account const & account_a, germ::raw_key const & prv_a, uint64_t sequence_a, std::shared_ptr<germ::tx> block_a) :
sequence (sequence_a),
block (block_a),
account (account_a),
signature (germ::sign_message (prv_a, account_a, hash ()))
{
}

germ::vote::vote (MDB_val const & value_a)
{
    germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value_a.mv_data), value_a.mv_size);
    auto error (germ::read (stream, account.bytes));
    assert (!error);
    error = germ::read (stream, signature.bytes);
    assert (!error);
    error = germ::read (stream, sequence);
    assert (!error);
    block = germ::deserialize_block (stream);
    assert (block != nullptr);
}

germ::uint256_union germ::vote::hash () const
{
    germ::uint256_union result;
    blake2b_state hash;
    blake2b_init (&hash, sizeof (result.bytes));
    blake2b_update (&hash, block->hash ().bytes.data (), sizeof (result.bytes));
    union
    {
        uint64_t qword;
        std::array<uint8_t, 8> bytes;
    };
    qword = sequence;
    blake2b_update (&hash, bytes.data (), sizeof (bytes));
    blake2b_final (&hash, result.bytes.data (), sizeof (result.bytes));
    return result;
}

void germ::vote::serialize (germ::stream & stream_a, germ::block_type)
{
    write (stream_a, account);
    write (stream_a, signature);
    write (stream_a, sequence);
    block->serialize (stream_a);
}

void germ::vote::serialize (germ::stream & stream_a)
{
    write (stream_a, account);
    write (stream_a, signature);
    write (stream_a, sequence);
    germ::serialize_block (stream_a, *block);
}

bool germ::vote::deserialize (germ::stream & stream_a)
{
    auto result (read (stream_a, account));
    if (result)
        return result;

    result = read (stream_a, signature);
    if (result)
        return result;

    result = read (stream_a, sequence);
    if (result)
        return result;

    block = germ::deserialize_block (stream_a, block_type ());
    result = block == nullptr;
    return result;
}

bool germ::vote::validate ()
{
    auto result (germ::validate_message (account, hash (), signature));
    return result;
}

germ::genesis::genesis ()
{
    boost::property_tree::ptree tree;
    std::stringstream istream (germ::genesis_block);
    boost::property_tree::read_json (istream, tree);
    auto block (germ::deserialize_block_json (tree));
    assert (block != nullptr);

#if GERM_STD
    open.reset (static_cast<germ::open_tx *> (block.release ()));
#else
    auto tx(static_cast<germ::tx *> (block.release ()));
    assert(tx != nullptr);

    open.reset (tx);
#endif
}

void germ::genesis::initialize (MDB_txn * transaction_a, germ::block_store & store_a) const
{

    auto hash_l (hash ());
    assert (store_a.latest_begin (transaction_a) == store_a.latest_end ());
    store_a.block_put (transaction_a, hash_l, *open);
    store_a.account_put (transaction_a, genesis_account, { hash_l, /*open->hash (),*/ open->hash (), std::numeric_limits<germ::uint128_t>::max (), germ::seconds_since_epoch (), 1 });
//    store_a.representation_put (transaction_a, genesis_account, std::numeric_limits<germ::uint128_t>::max ());
    store_a.checksum_put (transaction_a, 0, 0, hash_l);
    store_a.frontier_put (transaction_a, hash_l, genesis_account);
}

germ::block_hash germ::genesis::hash () const
{
    return open->hash ();
}
