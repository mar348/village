//
// Created by daoful on 18-8-1.
//

#include <src/node/bootstrap/bootstrap.h>

germ::pull_info::pull_info () :
account (0),
end (0),
attempts (0)
{
}

germ::pull_info::pull_info (germ::account const & account_a, germ::block_hash const & head_a, germ::block_hash const & end_a) :
account (account_a),
head (head_a),
end (end_a),
attempts (0)
{
}
