//
// Created by daoful on 18-7-26.
//

#ifndef SRC_EPOCHSTORE_H
#define SRC_EPOCHSTORE_H

#include <src/common.hpp>
#include <src/epoch_common.h>
#include <src/lib/epoch.h>
#include <src/lib/tx.h>

namespace germ
{

/**
 * Iterates the key/value pairs of a epoch block
 */
    class epoch_iterator
    {
    public:
        epoch_iterator (MDB_txn *, MDB_dbi);
        epoch_iterator (std::nullptr_t);
        epoch_iterator (MDB_txn *, MDB_dbi, MDB_val const &);
        epoch_iterator (germ::epoch_iterator &&);
        epoch_iterator (germ::epoch_iterator const &) = delete;
        ~epoch_iterator ();
        germ::epoch_iterator & operator++ ();
        //void next_dup ();
        germ::epoch_iterator & operator= (germ::epoch_iterator &&);
        germ::epoch_iterator & operator= (germ::epoch_iterator const &) = delete;
        std::pair<germ::mdb_val, germ::mdb_val> * operator-> ();
        bool operator== (germ::epoch_iterator const &) const;
        bool operator!= (germ::epoch_iterator const &) const;
        void clear ();
        MDB_cursor * cursor;
        std::pair<germ::mdb_val, germ::mdb_val> current;
    };

    /**
    * Manages block storage and iteration
    */
    class epoch_store
    {
    public:
        //constructor
        epoch_store (bool &, boost::filesystem::path const &, int lmdb_max_dbs = 128);

        //create a db with epoch bloch type
        MDB_dbi block_database (germ::block_type);

        //get the serilaized epoch block, and call block_put_raw to store it into DB
        void block_put (MDB_txn *, germ::epoch_hash const &, germ::epoch const &, germ::epoch_hash const & = germ::epoch_hash (0));
        //put an epoch block into DB
        void block_put_raw (MDB_txn *, MDB_dbi, germ::epoch_hash const &, MDB_val);

        //deserialize the epoch block got from "block_get_raw()"
        std::unique_ptr<germ::epoch> block_get (MDB_txn *, germ::epoch_hash const &);
        //get epoch block from DB
        MDB_val block_get_raw (MDB_txn *, germ::epoch_hash const &, germ::block_type &);

        void block_del (MDB_txn *, germ::epoch_hash const &);

        bool block_exists (MDB_txn *, germ::epoch_hash const &);

        germ::epoch_counts block_count (MDB_txn *);

        //get the hash of the argument's next block (successor)
        germ::epoch_hash block_successor (MDB_txn *, germ::epoch_hash const &);
        void block_successor_clear (MDB_txn *, germ::epoch_hash const &);
//        std::unique_ptr<germ::epoch> block_random (MDB_txn *);
//        std::unique_ptr<germ::epoch> block_random (MDB_txn *, MDB_dbi);

//        bool root_exists (MDB_txn *, germ::uint256_union const &);
//          void frontier_put (MDB_txn *, germ::epoch_hash const &, germ::account const &);
//          germ::account frontier_get (MDB_txn *, germ::epoch_hash const &);
//          void frontier_del (MDB_txn *, germ::epoch_hash const &);

//        void account_put (MDB_txn *, germ::account const &, germ::account_info const &);
//        bool account_get (MDB_txn *, germ::account const &, germ::account_info &);
//        void account_del (MDB_txn *, germ::account const &);
//        bool account_exists (MDB_txn *, germ::account const &);
//        size_t account_count (MDB_txn *);
        //epoch iterators
        germ::epoch_iterator latest_begin (MDB_txn *, germ::epoch_hash const &);
        germ::epoch_iterator latest_begin (MDB_txn *);
        germ::epoch_iterator latest_end ();

//        void block_info_put (MDB_txn *, germ::epoch_hash const &, germ::epoch_infos const &);
//        void block_info_del (MDB_txn *, germ::epoch_hash const &);
//        bool block_info_get (MDB_txn *, germ::epoch_hash const &, germ::epoch_infos &);
//        bool block_info_exists (MDB_txn *, germ::epoch_hash const &);

        //epoch info iterators
        germ::epoch_iterator block_info_begin (MDB_txn *, germ::epoch_hash const &);
        germ::epoch_iterator block_info_begin (MDB_txn *);
        germ::epoch_iterator block_info_end ();
        static size_t const block_info_max = 32;

        void checksum_put (MDB_txn *, uint64_t, uint8_t, germ::checksum const &);
        bool checksum_get (MDB_txn *, uint64_t, uint8_t, germ::checksum &);
        void checksum_del (MDB_txn *, uint64_t, uint8_t);

        void version_put (MDB_txn *, int);

        // Requires a write transaction
        germ::raw_key get_node_id (MDB_txn *);

        /** Deletes the node ID from the store */
        void delete_node_id (MDB_txn *);

        void clear (MDB_dbi);

        germ::mdb_env environment;
//        /**
//         * Maps head block to owning account
//         * germ::epoch_hash -> germ::account
//         */
//        MDB_dbi frontiers;

//        /**
//         * Maps account to account information, head, rep, open, balance, timestamp and block count.
//         * germ::account -> germ::epoch_hash, germ::epoch_hash, germ::epoch_hash, germ::amount, uint64_t, uint64_t
//         */
//        MDB_dbi accounts;

        /**
         * Maps epoch hash to epoch block.
         * germ::epoch_hash -> germ::epoch
         */
        MDB_dbi  epoch_blocks;

//        /**
//         * Maps epoch hash to timestamp, previous, merkle, signature.
//         * epoch_hash -> germ::uint64_t, germ::epoch_hash, (1, 2 ,3) germ::signature
//         */
//        MDB_dbi blocks_info;
        /**
         * Mapping of region to checksum.
         * (uint56_t, uint8_t) -> germ::epoch_hash
         */
        MDB_dbi checksum;

        /**
         * Meta information about block store, such as versions.
         * germ::uint256_union (arbitrary key) -> blob
         */
        MDB_dbi meta;
    };

}


#endif //SRC_EPOCHSTORE_H