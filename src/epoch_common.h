//
// Created by daoful on 18-7-27.
//

#ifndef SRC_EPOCH_COMMON_H
#define SRC_EPOCH_COMMON_H

#include <stddef.h>
#include <src/node/utility.hpp>
#include <src/lib/blocks.hpp>


namespace germ
{

    class epoch_counts
    {
    public:
        epoch_counts ();
        size_t sum ();
        size_t count;
    };

//    class epoch_infos
//    {
//    public:
//        epoch_infos ();
//        epoch_infos (MDB_val const &);
//        epoch_infos (uint64_t const timestamp_r, germ::epoch_hash const & prev_r, germ::epoch_hash const & merkle_r, germ::signature const & signature_r);
//        void serialize (germ::stream &) const;
//        bool deserialize (germ::stream &);
//        bool operator== (germ::epoch_infos const &) const;
//        germ::mdb_val val () const;
//
//        uint64_t    timestamp;
//        germ::epoch_hash    prev;
//        germ::epoch_hash    merkle;
//        germ::signature    signature;
//    };

}


#endif //SRC_EPOCH_COMMON_H