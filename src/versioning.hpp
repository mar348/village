#pragma once

#include <src/lib/blocks.hpp>
#include <src/node/utility.hpp>

namespace germ
{
class account_info_v1
{
public:
    account_info_v1 ();
    account_info_v1 (MDB_val const &);
    account_info_v1 (germ::account_info_v1 const &) = default;
    account_info_v1 (germ::block_hash const &, germ::block_hash const &, germ::amount const &, uint64_t);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    germ::mdb_val val () const;
    germ::block_hash head;
    germ::block_hash rep_block;
    germ::amount balance;
    uint64_t modified;
};
class pending_info_v3
{
public:
    pending_info_v3 ();
    pending_info_v3 (MDB_val const &);
    pending_info_v3 (germ::account const &, germ::amount const &, germ::account const &);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    bool operator== (germ::pending_info_v3 const &) const;
    germ::mdb_val val () const;
    germ::account source;
    germ::amount amount;
    germ::account destination;
};
// Latest information about an account
class account_info_v5
{
public:
    account_info_v5 ();
    account_info_v5 (MDB_val const &);
    account_info_v5 (germ::account_info_v5 const &) = default;
    account_info_v5 (germ::block_hash const &, germ::block_hash const &, germ::block_hash const &, germ::amount const &, uint64_t);
    void serialize (germ::stream &) const;
    bool deserialize (germ::stream &);
    germ::mdb_val val () const;
    germ::block_hash head;
//    germ::block_hash rep_block;
    germ::block_hash open_block;
    germ::amount balance;
    uint64_t modified;
};
}
