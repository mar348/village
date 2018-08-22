//
// Created by daoful on 18-7-26.
//

#include <queue>
#include <src/epochstore.h>
#include <src/versioning.hpp>
#include <src/lib/epoch.h>


namespace
{
/**
 * Fill in our predecessors
 */
    class set_predecessor : public germ::epoch_visitor
    {
    public:
        set_predecessor (MDB_txn * transaction_a, germ::epoch_store & store_a) :
                transaction (transaction_a),
                store (store_a)
        {
        }
        virtual ~set_predecessor () = default;
        void fill_value (germ::epoch const & epoch_r)
        {
            auto hash (epoch_r.hash ());
            germ::block_type type;
            auto value (store.block_get_raw (transaction, epoch_r.previous_epoch (), type));
            assert (value.mv_size != 0);
            std::vector<uint8_t> data (static_cast<uint8_t *> (value.mv_data), static_cast<uint8_t *> (value.mv_data) + value.mv_size);
            std::copy (hash.bytes.begin (), hash.bytes.end (), data.end () - hash.bytes.size ());
            store.block_put_raw (transaction, store.block_database (type), epoch_r.previous_epoch (), germ::mdb_val (data.size (), data.data ()));
        }
        void epoch_block (germ::epoch const & epoch_r) override
        {
            fill_value (epoch_r);
        }

        MDB_txn * transaction;
        germ::epoch_store & store;
    };
}

std::pair<germ::mdb_val, germ::mdb_val> * germ::epoch_iterator::operator-> ()
{
    return &current;
}

germ::epoch_iterator::epoch_iterator (MDB_txn * transaction_a, MDB_dbi db_a) :
        cursor (nullptr)
{
    auto status (mdb_cursor_open (transaction_a, db_a, &cursor));
    assert (status == 0);
    auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_FIRST));
    assert (status2 == 0 || status2 == MDB_NOTFOUND);
    if (status2 != MDB_NOTFOUND)
    {
        auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
        assert (status3 == 0 || status3 == MDB_NOTFOUND);
    }
    else
    {
        clear ();
    }
}

germ::epoch_iterator::epoch_iterator (std::nullptr_t) :
        cursor (nullptr)
{
}

germ::epoch_iterator::epoch_iterator (MDB_txn * transaction_a, MDB_dbi db_a, MDB_val const & val_a) :
        cursor (nullptr)
{
    auto status (mdb_cursor_open (transaction_a, db_a, &cursor));
    assert (status == 0);
    current.first.value = val_a;
    auto status2 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_SET_RANGE));
    assert (status2 == 0 || status2 == MDB_NOTFOUND);
    if (status2 != MDB_NOTFOUND)
    {
        auto status3 (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_GET_CURRENT));
        assert (status3 == 0 || status3 == MDB_NOTFOUND);
    }
    else
    {
        clear ();
    }
}

germ::epoch_iterator::epoch_iterator (germ::epoch_iterator && other_a)
{
    cursor = other_a.cursor;
    other_a.cursor = nullptr;
    current = other_a.current;
}

germ::epoch_iterator::~epoch_iterator ()
{
    if (cursor != nullptr)
    {
        mdb_cursor_close (cursor);
    }
}

germ::epoch_iterator & germ::epoch_iterator::operator++ ()
{
    assert (cursor != nullptr);
    auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_NEXT));
    if (status == MDB_NOTFOUND)
    {
        clear ();
    }
    return *this;
}

//void germ::epoch_iterator::next_dup ()
//{
//    assert (cursor != nullptr);
//    auto status (mdb_cursor_get (cursor, &current.first.value, &current.second.value, MDB_NEXT_DUP));
//    if (status == MDB_NOTFOUND)
//    {
//        clear ();
//    }
//}

germ::epoch_iterator & germ::epoch_iterator::operator= (germ::epoch_iterator && other_a)
{
    if (cursor != nullptr)
    {
        mdb_cursor_close (cursor);
    }
    cursor = other_a.cursor;
    other_a.cursor = nullptr;
    current = other_a.current;
    other_a.clear ();
    return *this;
}

bool germ::epoch_iterator::operator== (germ::epoch_iterator const & other_a) const
{
    auto result (current.first.data () == other_a.current.first.data ());
    assert (!result || (current.first.size () == other_a.current.first.size ()));
    assert (!result || (current.second.data () == other_a.current.second.data ()));
    assert (!result || (current.second.size () == other_a.current.second.size ()));
    return result;
}

bool germ::epoch_iterator::operator!= (germ::epoch_iterator const & other_a) const
{
    return !(*this == other_a);
}

void germ::epoch_iterator::clear ()
{
    current.first = germ::mdb_val ();
    current.second = germ::mdb_val ();
}

germ::epoch_iterator germ::epoch_store::block_info_begin (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
{
    germ::epoch_iterator result (transaction_a, epoch_blocks, germ::mdb_val (hash_a));
    return result;
}

germ::epoch_iterator germ::epoch_store::block_info_begin (MDB_txn * transaction_a)
{
    germ::epoch_iterator result (transaction_a, epoch_blocks);
    return result;
}

germ::epoch_iterator germ::epoch_store::block_info_end ()
{
    germ::epoch_iterator result (nullptr);
    return result;
}

/* ------------------------------------------- epoch_store class --------------------------------------------------------------------------------------*/
//constructor
germ::epoch_store::epoch_store (bool & error_a, boost::filesystem::path const & path_a, int lmdb_max_dbs) :
        environment (error_a, path_a, lmdb_max_dbs),
        //frontiers (0),
        //accounts (0),
        //blocks_info (0),
        epoch_blocks (0),
        checksum (0)
{
    if (!error_a)
    {
        germ::transaction transaction (environment, nullptr, true);
        //error_a |= mdb_dbi_open (transaction, "frontiers", MDB_CREATE, &frontiers) != 0;
        //error_a |= mdb_dbi_open (transaction, "accounts", MDB_CREATE, &accounts) != 0;
        //error_a |= mdb_dbi_open (transaction, "blocks_info", MDB_CREATE, &blocks_info) != 0;
        error_a |= mdb_dbi_open (transaction, "checksum", MDB_CREATE, &checksum) != 0;
        error_a |= mdb_dbi_open (transaction, "meta", MDB_CREATE, &meta) != 0;
        if (!error_a)
        {
            checksum_put (transaction, 0, 0, 0);
        }
    }
}

//create a db with epoch bloch type, if type is not epoch,
MDB_dbi germ::epoch_store::block_database (germ::block_type type_a)
{
    MDB_dbi result;
    switch (type_a)
    {
        case germ::block_type::epoch:
            result = epoch_blocks;
            break;
        default:
            assert (false);
            break;
    }
    return result;
}

//get the serilaized epoch block, and call block_put_raw to store it into DB
void germ::epoch_store::block_put (MDB_txn * transaction_a, germ::epoch_hash const & hash_a, germ::epoch const & block_a, germ::epoch_hash const & successor_a)
{
    assert (successor_a.is_zero () || block_exists (transaction_a, successor_a));
    std::vector<uint8_t> vector;
    {
        germ::vectorstream stream (vector);
        block_a.serialize (stream);
        germ::write (stream, successor_a.bytes);
    }
    //             MDB_txn *      MDB_dbi                          germ::epoch_hash           MDB_val value_a)
    block_put_raw (transaction_a, block_database (block_a.type ()), hash_a, { vector.size (), vector.data () });
    set_predecessor predecessor (transaction_a, *this);
    block_a.visit (predecessor);
    assert (block_a.previous_epoch ().is_zero () || block_successor (transaction_a, block_a.previous_epoch ()) == hash_a);
}

//put an epoch block into DB
void germ::epoch_store::block_put_raw (MDB_txn * transaction_a, MDB_dbi database_a, germ::epoch_hash const & hash_a, MDB_val value_a)
{
    auto status2 (mdb_put (transaction_a, database_a, germ::mdb_val (hash_a), &value_a, 0));
    assert (status2 == 0);
}

//deserialize the epoch block got from "block_get_raw()"
std::unique_ptr<germ::epoch> germ::epoch_store::block_get (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
{
    germ::block_type type;
    auto value (block_get_raw (transaction_a, hash_a, type));
    std::unique_ptr<germ::epoch> result;
    if (value.mv_size != 0)
    {
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data), value.mv_size);
        result = germ::deserialize_epoch (stream, type);
        assert (result != nullptr);
    }
    return result;
}

//get epoch block from DB
MDB_val germ::epoch_store::block_get_raw (MDB_txn * transaction_a, germ::epoch_hash const & hash_a, germ::block_type & type_a)
{
    germ::mdb_val result;
    auto status_epoch (mdb_get (transaction_a, epoch_blocks, germ::mdb_val (hash_a), result));
    assert (status_epoch == 0 || status_epoch == MDB_NOTFOUND);
    if (status_epoch == 0)
    {
        type_a = germ::block_type::epoch;
        return result;
    }
    return result;
}

//given a hash, delete a epoch block
void germ::epoch_store::block_del (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
{
    auto status_epoch (mdb_del (transaction_a, epoch_blocks, germ::mdb_val (hash_a), nullptr));
    assert (status_epoch == 0 || status_epoch == MDB_NOTFOUND);
    if (status_epoch == 0)
        return;
}

//returns true if the given hash's epoch is in the DB
bool germ::epoch_store::block_exists (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
{
    auto exists (true);
    germ::mdb_val junk;
    auto status_epoch (mdb_get (transaction_a, epoch_blocks, germ::mdb_val (hash_a), junk));
    assert (status_epoch == 0 || status_epoch == MDB_NOTFOUND);
    exists = status_epoch == 0;
    if (exists)
        return exists;
    return exists;
}


void germ::epoch_store::version_put (MDB_txn * transaction_a, int version_a)
{
    germ::uint256_union version_key (1);
    germ::uint256_union version_value (version_a);
    auto status (mdb_put (transaction_a, meta, germ::mdb_val (version_key), germ::mdb_val (version_value), 0));
    assert (status == 0);
}

germ::raw_key germ::epoch_store::get_node_id (MDB_txn * transaction_a)
{
    germ::uint256_union node_id_mdb_key (3);
    germ::raw_key node_id;
    germ::mdb_val value;
    auto error (mdb_get (transaction_a, meta, germ::mdb_val (node_id_mdb_key), value));
    if (!error)
    {
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
        error = germ::read (stream, node_id.data);
        assert (!error);
    }
    if (error)
    {
        germ::random_pool.GenerateBlock (node_id.data.bytes.data (), node_id.data.bytes.size ());
        error = mdb_put (transaction_a, meta, germ::mdb_val (node_id_mdb_key), germ::mdb_val (node_id.data), 0);
    }
    assert (!error);
    return node_id;
}

void germ::epoch_store::delete_node_id (MDB_txn * transaction_a)
{
    germ::uint256_union node_id_mdb_key (3);
    auto error (mdb_del (transaction_a, meta, germ::mdb_val (node_id_mdb_key), nullptr));
    assert (!error || error == MDB_NOTFOUND);
}

void germ::epoch_store::clear (MDB_dbi db_a)
{
    germ::transaction transaction (environment, nullptr, true);
    auto status (mdb_drop (transaction, db_a, 0));
    assert (status == 0);
}

//block_random was meant for rep_query, now is taken out
//std::unique_ptr<germ::epoch> germ::epoch_store::block_random (MDB_txn * transaction_a, MDB_dbi database)
//{
//    germ::epoch_hash hash;
//    germ::random_pool.GenerateBlock (hash.bytes.data (), hash.bytes.size ());
//    germ::epoch_iterator existing (transaction_a, database, germ::mdb_val (hash));
//    if (existing == germ::epoch_iterator (nullptr))
//    {
//        existing = germ::epoch_iterator (transaction_a, database);
//    }
//    assert (existing != germ::epoch_iterator (nullptr));
//    return block_get (transaction_a, germ::epoch_hash (existing->first.uint256 ()));
//}
//
//std::unique_ptr<germ::epoch> germ::epoch_store::block_random (MDB_txn * transaction_a)
//{
//    auto count (block_count (transaction_a));
//    auto region (germ::random_pool.GenerateWord32 (0, count.sum () - 1));
//    std::unique_ptr<germ::epoch> result;
//    if (region < count.epoch)
//    {
//        result = block_random (transaction_a, epoch_blocks);
//    }
//    else
//    {
//        region -= count.epoch;
//
//        // other type
//    }
//    return result;
//}

// returns the hash of next epoch block (epoch# +1)
germ::epoch_hash germ::epoch_store::block_successor (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
{
    germ::block_type type;
    auto value (block_get_raw (transaction_a, hash_a, type));
    germ::epoch_hash result;
    if (value.mv_size != 0)
    {
        assert (value.mv_size >= result.bytes.size ());
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value.mv_data) + value.mv_size - result.bytes.size (), result.bytes.size ());
        auto error (germ::read (stream, result.bytes));
        assert (!error);
    }
    else
    {
        result.clear ();
    }
    return result;
}

void germ::epoch_store::block_successor_clear (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
{
    auto block (block_get (transaction_a, hash_a));
    block_put (transaction_a, hash_a, *block);
}





//retue
germ::epoch_counts germ::epoch_store::block_count (MDB_txn * transaction_a)
{
    germ::epoch_counts result;
    MDB_stat epoch_stats;
    auto status1 (mdb_stat (transaction_a, epoch_blocks, &epoch_stats));
    assert (status1 == 0);

    result.count = epoch_stats.ms_entries;

    return result;
}

//bool germ::epoch_store::root_exists (MDB_txn * transaction_a, germ::uint256_union const & root_a)
//{
//    return block_exists (transaction_a, root_a) || account_exists (transaction_a, root_a);
//}

//void germ::epoch_store::account_del (MDB_txn * transaction_a, germ::account const & account_a)
//{
//    auto status (mdb_del (transaction_a, accounts, germ::mdb_val (account_a), nullptr));
//    assert (status == 0);
//}
//
//bool germ::epoch_store::account_exists (MDB_txn * transaction_a, germ::account const & account_a)
//{
//    auto iterator (latest_begin (transaction_a, account_a));
//    return iterator != germ::epoch_iterator (nullptr) && germ::account (iterator->first.uint256 ()) == account_a;
//}
//
//bool germ::epoch_store::account_get (MDB_txn * transaction_a, germ::account const & account_a, germ::account_info & info_a)
//{
//    germ::mdb_val value;
//    auto status (mdb_get (transaction_a, accounts, germ::mdb_val (account_a), value));
//    assert (status == 0 || status == MDB_NOTFOUND);
//    bool result;
//    if (status == MDB_NOTFOUND)
//    {
//        result = false;
//    }
//    else
//    {
//        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
//        result = info_a.deserialize (stream);
//        assert (result);
//    }
//    return !result;
//}

//frontier stuff commented out for now
//void germ::epoch_store::frontier_put (MDB_txn * transaction_a, germ::epoch_hash const & block_a, germ::account const & account_a)
//{
//    auto status (mdb_put (transaction_a, frontiers, germ::mdb_val (block_a), germ::mdb_val (account_a), 0));
//    assert (status == 0);
//}

//germ::account germ::epoch_store::frontier_get (MDB_txn * transaction_a, germ::epoch_hash const & block_a)
//{
//    germ::mdb_val value;
//    auto status (mdb_get (transaction_a, frontiers, germ::mdb_val (block_a), value));
//    assert (status == 0 || status == MDB_NOTFOUND);
//    germ::account result (0);
//    if (status == 0)
//    {
//        result = value.uint256 ();
//    }
//    return result;
//}

//void germ::epoch_store::frontier_del (MDB_txn * transaction_a, germ::epoch_hash const & block_a)
//{
//    auto status (mdb_del (transaction_a, frontiers, germ::mdb_val (block_a), nullptr));
//    assert (status == 0);
//}

//size_t germ::epoch_store::account_count (MDB_txn * transaction_a)
//{
//    MDB_stat frontier_stats;
//    auto status (mdb_stat (transaction_a, accounts, &frontier_stats));
//    assert (status == 0);
//    auto result (frontier_stats.ms_entries);
//    return result;
//}

//void germ::epoch_store::account_put (MDB_txn * transaction_a, germ::account const & account_a, germ::account_info const & info_a)
//{
//    auto status (mdb_put (transaction_a, accounts, germ::mdb_val (account_a), info_a.val (), 0));
//    assert (status == 0);
//}

//void germ::epoch_store::block_info_put (MDB_txn * transaction_a, germ::epoch_hash const & hash_a, germ::epoch_infos const & block_info_a)
//{
//    auto status (mdb_put (transaction_a, blocks_info, germ::mdb_val (hash_a), block_info_a.val (), 0));
//    assert (status == 0);
//}
//
//void germ::epoch_store::block_info_del (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
//{
//    auto status (mdb_del (transaction_a, blocks_info, germ::mdb_val (hash_a), nullptr));
//    assert (status == 0);
//}
//
//bool germ::epoch_store::block_info_exists (MDB_txn * transaction_a, germ::epoch_hash const & hash_a)
//{
//    auto iterator (block_info_begin (transaction_a, hash_a));
//    return iterator != germ::epoch_iterator (nullptr) && germ::epoch_hash (iterator->first.uint256 ()) == hash_a;
//}
//
////TODO: change this to use epoch object instead of epoch_infos
//bool germ::epoch_store::block_info_get (MDB_txn * transaction_a, germ::epoch_hash const & hash_a, germ::epoch & block_info_a)
//{
//    germ::mdb_val value;
//    auto status (mdb_get (transaction_a, blocks_info, germ::mdb_val (hash_a), value));
//    assert (status == 0 || status == MDB_NOTFOUND);
//    bool result;
//    if (status == MDB_NOTFOUND)
//    {
//        result = false;
//    }
//    else
//    {
//        result = true;
//        assert (value.size () == block_info_a.size());
//        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
//
//        auto error1 (germ::read (stream, block_info_a.timestamp));
//        assert (!error1);
//        auto error2 (germ::read (stream, block_info_a.prev().bytes));
//        assert (!error2);
//
//
//        auto error3 (germ::read (stream, block_info_a.txs()));
//        assert (!error3);
//
//
//
//
//        auto error4 (germ::read (stream, block_info_a.signature().bytes));
//        assert (!error4);
//    }
//    return result;
//}


void germ::epoch_store::checksum_put (MDB_txn * transaction_a, uint64_t prefix, uint8_t mask, germ::uint256_union const & hash_a)
{
    assert ((prefix & 0xff) == 0);
    uint64_t key (prefix | mask);
    auto status (mdb_put (transaction_a, checksum, germ::mdb_val (sizeof (key), &key), germ::mdb_val (hash_a), 0));
    assert (status == 0);
}

bool germ::epoch_store::checksum_get (MDB_txn * transaction_a, uint64_t prefix, uint8_t mask, germ::uint256_union & hash_a)
{
    assert ((prefix & 0xff) == 0);
    uint64_t key (prefix | mask);
    germ::mdb_val value;
    auto status (mdb_get (transaction_a, checksum, germ::mdb_val (sizeof (key), &key), value));
    assert (status == 0 || status == MDB_NOTFOUND);
    bool result;
    if (status == 0)
    {
        result = true;
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (value.data ()), value.size ());
        auto error (germ::read (stream, hash_a));
        assert (!error);
    }
    else
    {
        result = false;
    }
    return result;
}

void germ::epoch_store::checksum_del (MDB_txn * transaction_a, uint64_t prefix, uint8_t mask)
{
    assert ((prefix & 0xff) == 0);
    uint64_t key (prefix | mask);
    auto status (mdb_del (transaction_a, checksum, germ::mdb_val (sizeof (key), &key), nullptr));
    assert (status == 0);
}

germ::epoch_iterator germ::epoch_store::latest_begin (MDB_txn * transaction_a, germ::epoch_hash const & epoch_r)
{
    germ::epoch_iterator result (transaction_a, epoch_blocks, germ::mdb_val (epoch_r));
    return result;
}

germ::epoch_iterator germ::epoch_store::latest_begin (MDB_txn * transaction_a)
{
    germ::epoch_iterator result (transaction_a, epoch_blocks);
    return result;
}

germ::epoch_iterator germ::epoch_store::latest_end ()
{
    germ::epoch_iterator result (nullptr);
    return result;
}