//
// Created by daoful on 18-8-1.
//

#ifndef SRC_BOOTSTRAP_H
#define SRC_BOOTSTRAP_H

#include <src/node/common.hpp>

#include <atomic>
#include <future>
#include <queue>
#include <stack>
#include <unordered_set>

namespace germ
{
enum class sync_result
{
    success,
    error,
    fork
};

/**
 * The length of every message header, parsed by germ::message::read_header ()
 * The 2 here represents the size of a std::bitset<16>, which is 2 chars long normally
 */
static const int bootstrap_message_header_size = sizeof (germ::message_header::magic_number) + sizeof (uint8_t) + sizeof (uint8_t) + sizeof (uint8_t) + sizeof (germ::message_type) + 2;

class pull_info
{
public:
    pull_info ();
    pull_info (germ::account const &, germ::block_hash const &, germ::block_hash const &);
    germ::account account;
    germ::block_hash head;
    germ::block_hash end;
    unsigned attempts;
};



}


#endif //SRC_BOOTSTRAP_H
