#include <src/blockstore.hpp>
#include <src/ledger.hpp>
#include <src/node/common.hpp>
#include <src/node/stats.hpp>
#include <src/lib/tx.h>

namespace
{
/**
 * Roll back the visited block
 */
class rollback_visitor : public germ::block_visitor
{
public:
    rollback_visitor (MDB_txn * transaction_a, germ::ledger & ledger_a) :
    transaction (transaction_a),
    ledger (ledger_a)
    {
    }
    virtual ~rollback_visitor () = default;
    void send_block (germ::send_block const & tx) override
    {
        auto hash (tx.hash ());
        germ::pending_info pending;
        germ::pending_key key (tx.hashables.destination, hash);
        while ( ledger.store.pending_get (transaction, key, pending) )
        {
            ledger.rollback (transaction, ledger.latest (transaction, tx.hashables.destination));
        }
        germ::account_info info;
        auto found (ledger.store.account_get (transaction, pending.source, info));
        assert (found);
        ledger.store.pending_del (transaction, key);
//        ledger.store.representation_add (transaction, ledger.representative (transaction, hash), pending.amount.number ());
        ledger.change_latest (transaction, pending.source, tx.hashables.previous, /*info.rep_block,*/ ledger.balance (transaction, tx.hashables.previous), info.block_count - 1);
        ledger.store.block_del (transaction, hash);
        ledger.store.frontier_del (transaction, hash);
        ledger.store.frontier_put (transaction, tx.hashables.previous, pending.source);
        ledger.store.block_successor_clear (transaction, tx.hashables.previous);
        if (!(info.block_count % ledger.store.block_info_max))
        {
            ledger.store.block_info_del (transaction, hash);
        }
        ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::send);
    }
    void receive_block (germ::receive_block const & tx) override
    {
        auto hash (tx.hash ());
//        auto representative (ledger.representative (transaction, tx.hashables.previous));
        auto amount (ledger.amount (transaction, tx.hashables.source));
        auto destination_account (ledger.account (transaction, hash));
        auto source_account (ledger.account (transaction, tx.hashables.source));
        germ::account_info info;
        auto found (ledger.store.account_get (transaction, destination_account, info));
        assert (found);
//        ledger.store.representation_add (transaction, ledger.representative (transaction, hash), 0 - amount);
        ledger.change_latest (transaction, destination_account, tx.hashables.previous, /*representative,*/ ledger.balance (transaction, tx.hashables.previous), info.block_count - 1);
        ledger.store.block_del (transaction, hash);
        ledger.store.pending_put (transaction, germ::pending_key (destination_account, tx.hashables.source), { source_account, amount });
        ledger.store.frontier_del (transaction, hash);
        ledger.store.frontier_put (transaction, tx.hashables.previous, destination_account);
        ledger.store.block_successor_clear (transaction, tx.hashables.previous);
        if (!(info.block_count % ledger.store.block_info_max))
        {
            ledger.store.block_info_del (transaction, hash);
        }
        ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::receive);
    }
    void open_block (germ::open_block const & tx) override
    {
        auto hash (tx.hash ());
        auto amount (ledger.amount (transaction, tx.hashables.source));
        auto destination_account (ledger.account (transaction, hash));
        auto source_account (ledger.account (transaction, tx.hashables.source));
//        ledger.store.representation_add (transaction, ledger.representative (transaction, hash), 0 - amount);
        ledger.change_latest (transaction, destination_account, 0, /*0,*/ 0, 0);
        ledger.store.block_del (transaction, hash);
        ledger.store.pending_put (transaction, germ::pending_key (destination_account, tx.hashables.source), { source_account, amount });
        ledger.store.frontier_del (transaction, hash);
        ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::open);
    }
    void change_block (germ::change_block const & tx) override
    {
        auto hash (tx.hash ());
//        auto representative (ledger.representative (transaction, tx.hashables.previous));
        auto account (ledger.account (transaction, tx.hashables.previous));
        germ::account_info info;
        auto found (ledger.store.account_get (transaction, account, info));
        assert (found);
        auto balance (ledger.balance (transaction, tx.hashables.previous));
//        ledger.store.representation_add (transaction, representative, balance);
//        ledger.store.representation_add (transaction, hash, 0 - balance);
        ledger.store.block_del (transaction, hash);
        ledger.change_latest (transaction, account, tx.hashables.previous, /*representative,*/ info.balance, info.block_count - 1);
        ledger.store.frontier_del (transaction, hash);
        ledger.store.frontier_put (transaction, tx.hashables.previous, account);
        ledger.store.block_successor_clear (transaction, tx.hashables.previous);
        if (!(info.block_count % ledger.store.block_info_max))
        {
            ledger.store.block_info_del (transaction, hash);
        }
        ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::change);
    }
    void state_block (germ::state_block const & tx) override
    {
        auto hash (tx.hash ());
//        germ::block_hash representative (0);
//        if (!tx.hashables.previous.is_zero ())
//        {
//            representative = ledger.representative (transaction, tx.hashables.previous);
//        }
        auto balance (ledger.balance (transaction, tx.hashables.previous));
        auto is_send (tx.hashables.balance < balance);
        // Add in amount delta
//        ledger.store.representation_add (transaction, hash, 0 - tx.hashables.balance.number ());
//        if (!representative.is_zero ())
//        {
//            // Move existing representation
//            ledger.store.representation_add (transaction, representative, balance);
//        }

        if (is_send)
        {
            germ::pending_key key (tx.hashables.link, hash);
            while (!ledger.store.pending_exists (transaction, key))
            {
                ledger.rollback (transaction, ledger.latest (transaction, tx.hashables.link));
            }
            ledger.store.pending_del (transaction, key);
            ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::send);
        }
        else if (!tx.hashables.link.is_zero ())
        {
            germ::pending_info info (ledger.account (transaction, tx.hashables.link), tx.hashables.balance.number () - balance);
            ledger.store.pending_put (transaction, germ::pending_key (tx.hashables.account, tx.hashables.link), info);
            ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::receive);
        }

        germ::account_info info;
        auto found (ledger.store.account_get (transaction, tx.hashables.account, info));
        assert (found);
        ledger.change_latest (transaction, tx.hashables.account, tx.hashables.previous, /*representative,*/ balance, info.block_count - 1);

        auto previous (ledger.store.block_get (transaction, tx.hashables.previous));
        if (previous != nullptr)
        {
            ledger.store.block_successor_clear (transaction, tx.hashables.previous);
            if (previous->type () < germ::block_type::state)
            {
                ledger.store.frontier_put (transaction, tx.hashables.previous, tx.hashables.account);
            }
        }
        else
        {
            ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::open);
        }
        ledger.store.block_del (transaction, hash);
    }
    void tx(germ::tx const& tx)
    {
        germ::block_type tx_type = tx.type();
        if (tx_type == germ::block_type::send)
        {
            auto hash (tx.hash ());
            germ::pending_info pending;
            germ::pending_key key (tx.destination_, hash);
            while ( ledger.store.pending_get (transaction, key, pending) )
            {
                ledger.rollback (transaction, ledger.latest (transaction, tx.destination_));
            }
            germ::account_info info;
            auto found (ledger.store.account_get (transaction, pending.source, info));
            assert (found);
            ledger.store.pending_del (transaction, key);
//            ledger.store.representation_add (transaction, ledger.representative (transaction, hash), pending.amount.number ());
            ledger.change_latest (transaction, pending.source, tx.previous_, /*info.rep_block,*/ ledger.balance (transaction, tx.previous_), info.block_count - 1);
            ledger.store.block_del (transaction, hash);
            ledger.store.frontier_del (transaction, hash);
            ledger.store.frontier_put (transaction, tx.previous_, pending.source);
            ledger.store.block_successor_clear (transaction, tx.previous_);
            if (!(info.block_count % ledger.store.block_info_max))
            {
                ledger.store.block_info_del (transaction, hash);
            }
            ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::send);
        }
        else if (tx_type == germ::block_type::vote)
        {

        }
        else if (tx_type == germ::block_type::receive)
        {
            auto hash (tx.hash ());
//        auto representative (ledger.representative (transaction, tx.hashables.previous));
            auto amount (ledger.amount (transaction, tx.source_));
            auto destination_account (ledger.account (transaction, hash));
            auto source_account (ledger.account (transaction, tx.source_));
            germ::account_info info;
            auto found (ledger.store.account_get (transaction, destination_account, info));
            assert (found);
//        ledger.store.representation_add (transaction, ledger.representative (transaction, hash), 0 - amount);
            ledger.change_latest (transaction, destination_account, tx.previous_, /*representative,*/ ledger.balance (transaction, tx.previous_), info.block_count - 1);
            ledger.store.block_del (transaction, hash);
            ledger.store.pending_put (transaction, germ::pending_key (destination_account, tx.source_), { source_account, amount });
            ledger.store.frontier_del (transaction, hash);
            ledger.store.frontier_put (transaction, tx.previous_, destination_account);
            ledger.store.block_successor_clear (transaction, tx.previous_);
            if (!(info.block_count % ledger.store.block_info_max))
            {
                ledger.store.block_info_del (transaction, hash);
            }
            ledger.stats.inc (germ::stat::type::rollback, germ::stat::detail::receive);
        }
    }


    MDB_txn * transaction;
    germ::ledger & ledger;
};

class ledger_processor : public germ::block_visitor
{
public:
    ledger_processor (germ::ledger &, MDB_txn *);
    virtual ~ledger_processor () = default;
    void send_block (germ::send_block const &) override;
    void receive_block (germ::receive_block const &) override;
    void open_block (germ::open_block const &) override;
    void change_block (germ::change_block const &) override;
    void state_block (germ::state_block const &) override;
    void state_block_impl (germ::state_block const &);
    void tx (germ::tx const & tx) override;
    germ::ledger & ledger;
    MDB_txn * transaction;
    germ::process_return result;
};

void ledger_processor::state_block (germ::state_block const & tx)
{
    state_block_impl (tx);
}

void ledger_processor::state_block_impl (germ::state_block const & tx)
{
    auto hash (tx.hash ());
    auto existing (ledger.store.block_exists (transaction, hash));
    result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block before? (Unambiguous)
    if (result.code != germ::process_result::progress)
        return;

    result.code = validate_message (tx.hashables.account, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is this block signed correctly (Unambiguous)
    if (result.code != germ::process_result::progress)
        return;

    result.code = tx.hashables.account.is_zero () ? germ::process_result::opened_burn_account : germ::process_result::progress; // Is this for the burn account? (Unambiguous)
    if (result.code == germ::process_result::progress)
    {
        germ::account_info info;
        result.amount = tx.hashables.balance;
        auto is_send (false);
        auto account_found (ledger.store.account_get (transaction, tx.hashables.account, info));
        if (account_found)
        {
            // Account already exists
            result.code = tx.hashables.previous.is_zero () ? germ::process_result::fork : germ::process_result::progress; // Has this account already been opened? (Ambigious)
            if (result.code == germ::process_result::progress)
            {
                result.code = ledger.store.block_exists (transaction, tx.hashables.previous) ? germ::process_result::progress : germ::process_result::gap_previous; // Does the previous block exist in the ledger? (Unambigious)
                if (result.code == germ::process_result::progress)
                {
                    is_send = tx.hashables.balance < info.balance;
                    result.amount = is_send ? (info.balance.number () - result.amount.number ()) : (result.amount.number () - info.balance.number ());
                    result.code = tx.hashables.previous == info.head ? germ::process_result::progress : germ::process_result::fork; // Is the previous block the account's head block? (Ambigious)
                }
            }
        }
        else
        {
            // Account does not yet exists
            result.code = tx.previous ().is_zero () ? germ::process_result::progress : germ::process_result::gap_previous; // Does the first block in an account yield 0 for previous() ? (Unambigious)
            if (result.code == germ::process_result::progress)
            {
                result.code = !tx.hashables.link.is_zero () ? germ::process_result::progress : germ::process_result::gap_source; // Is the first block receiving from a send ? (Unambigious)
            }
        }
        if (result.code == germ::process_result::progress)
        {
            if (!is_send)
            {
                if (!tx.hashables.link.is_zero ())
                {
                    result.code = ledger.store.block_exists (transaction, tx.hashables.link) ? germ::process_result::progress : germ::process_result::gap_source; // Have we seen the source block already? (Harmless)
                    if (result.code == germ::process_result::progress)
                    {
                        germ::pending_key key (tx.hashables.account, tx.hashables.link);
                        germ::pending_info pending;
                        result.code = (!ledger.store.pending_get (transaction, key, pending)) ? germ::process_result::unreceivable : germ::process_result::progress; // Has this source already been received (Malformed)
                        if (result.code == germ::process_result::progress)
                        {
                            result.code = result.amount == pending.amount ? germ::process_result::progress : germ::process_result::balance_mismatch;
                        }
                    }
                }
                else
                {
                    // If there's no link, the balance must remain the same, only the representative can change
                    result.code = result.amount.is_zero () ? germ::process_result::progress : germ::process_result::balance_mismatch;
                }
            }
        }
        if (result.code == germ::process_result::progress)
        {
            ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::state_block);
            result.state_is_send = is_send;
//            ledger.store.block_put (transaction, hash, tx);

//            if (!info.rep_block.is_zero ())
//            {
//                // Move existing representation
//                ledger.store.representation_add (transaction, info.rep_block, 0 - info.balance.number ());
//            }
//            // Add in amount delta
//            ledger.store.representation_add (transaction, hash, tx.hashables.balance.number ());

            if (is_send)
            {
                germ::pending_key key (tx.hashables.link, hash);
                germ::pending_info info (tx.hashables.account, result.amount.number ());
                ledger.store.pending_put (transaction, key, info);
            }
            else if (!tx.hashables.link.is_zero ())
            {
                ledger.store.pending_del (transaction, germ::pending_key (tx.hashables.account, tx.hashables.link));
            }

            ledger.change_latest (transaction, tx.hashables.account, hash, /*hash,*/ tx.hashables.balance, info.block_count + 1, true);
            if (!ledger.store.frontier_get (transaction, info.head).is_zero ())
            {
                ledger.store.frontier_del (transaction, info.head);
            }
            // Frontier table is unnecessary for state blocks and this also prevents old blocks from being inserted on top of state blocks
            result.account = tx.hashables.account;
        }
    }
}

void ledger_processor::change_block (germ::change_block const & tx)
{
//    auto hash (tx.hash ());
//    auto existing (ledger.store.block_exists (transaction, hash));
//    result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block before? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto previous (ledger.store.block_get (transaction, tx.hashables.previous));
//    result.code = previous != nullptr ? germ::process_result::progress : germ::process_result::gap_previous; // Have we seen the previous block already? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = tx.valid_predecessor (*previous) ? germ::process_result::progress : germ::process_result::block_position;
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto account (ledger.store.frontier_get (transaction, tx.hashables.previous));
//    result.code = account.is_zero () ? germ::process_result::fork : germ::process_result::progress;
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::account_info info;
//    auto latest_found (ledger.store.account_get (transaction, account, info));
//    assert (latest_found);
//    assert (info.head == tx.hashables.previous);
//    result.code = validate_message (account, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is this block signed correctly (Malformed)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    ledger.store.block_put (transaction, hash, tx);
//    auto balance (ledger.balance (transaction, tx.hashables.previous));
////    ledger.store.representation_add (transaction, hash, balance);
////    ledger.store.representation_add (transaction, info.rep_block, 0 - balance);
//    ledger.change_latest (transaction, account, hash, /*hash,*/ info.balance, info.block_count + 1);
//    ledger.store.frontier_del (transaction, tx.hashables.previous);
//    ledger.store.frontier_put (transaction, hash, account);
//    result.account = account;
//    result.amount = 0;
//    ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::change);
}

void ledger_processor::send_block (germ::send_block const & tx)
{
//    auto hash (tx.hash ());
//    auto existing (ledger.store.block_exists (transaction, hash));
//    result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block before? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto previous (ledger.store.block_get (transaction, tx.hashables.previous));
//    result.code = previous != nullptr ? germ::process_result::progress : germ::process_result::gap_previous; // Have we seen the previous block already? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = tx.valid_predecessor (*previous) ? germ::process_result::progress : germ::process_result::block_position;
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto account (ledger.store.frontier_get (transaction, tx.hashables.previous));
//    result.code = account.is_zero () ? germ::process_result::fork : germ::process_result::progress;
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = validate_message (account, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is this block signed correctly (Malformed)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::account_info info;
//    auto latest_found (ledger.store.account_get (transaction, account, info));
//    assert (latest_found);
//    assert (info.head == tx.hashables.previous);
//    result.code = info.balance.number () >= tx.hashables.balance.number () ? germ::process_result::progress : germ::process_result::negative_spend; // Is this trying to spend a negative amount (Malicious)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto amount (info.balance.number () - tx.hashables.balance.number ());
////    ledger.store.representation_add (transaction, info.rep_block, 0 - amount);
//    ledger.store.block_put (transaction, hash, tx);
//    ledger.change_latest (transaction, account, hash, /*info.rep_block,*/ tx.hashables.balance, info.block_count + 1);
//    ledger.store.pending_put (transaction, germ::pending_key (tx.hashables.destination, hash), { account, amount });
//    ledger.store.frontier_del (transaction, tx.hashables.previous);
//    ledger.store.frontier_put (transaction, hash, account);
//    result.account = account;
//    result.amount = amount;
//    result.pending_account = tx.hashables.destination;
//    ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::send);
}

void ledger_processor::receive_block (germ::receive_block const & tx)
{
//    auto hash (tx.hash ());
//    auto existing (ledger.store.block_exists (transaction, hash));
//    result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block already?  (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto previous (ledger.store.block_get (transaction, tx.hashables.previous));
//    result.code = previous != nullptr ? germ::process_result::progress : germ::process_result::gap_previous;
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = tx.valid_predecessor (*previous) ? germ::process_result::progress : germ::process_result::block_position;
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = ledger.store.block_exists (transaction, tx.hashables.source) ? germ::process_result::progress : germ::process_result::gap_source; // Have we seen the source block already? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto account (ledger.store.frontier_get (transaction, tx.hashables.previous));
//    result.code = account.is_zero () ? germ::process_result::gap_previous : germ::process_result::progress; //Have we seen the previous block? No entries for account at all (Harmless)
//    if (result.code != germ::process_result::progress)
//    {
//        result.code = ledger.store.block_exists (transaction, tx.hashables.previous) ? germ::process_result::fork : germ::process_result::gap_previous; // If we have the block but it's not the latest we have a signed fork (Malicious)
//        return;
//    }
//
//    result.code = germ::validate_message (account, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is the signature valid (Malformed)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::account_info info;
//    ledger.store.account_get (transaction, account, info);
//    result.code = info.head == tx.hashables.previous ? germ::process_result::progress : germ::process_result::gap_previous; // Block doesn't immediately follow latest block (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::pending_key key (account, tx.hashables.source);
//    germ::pending_info pending;
//    result.code = (!ledger.store.pending_get (transaction, key, pending)) ? germ::process_result::unreceivable : germ::process_result::progress; // Has this source already been received (Malformed)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto new_balance (info.balance.number () + pending.amount.number ());
//    germ::account_info source_info;
//    auto found (ledger.store.account_get (transaction, pending.source, source_info));
//    assert (found);
//    ledger.store.pending_del (transaction, key);
//    ledger.store.block_put (transaction, hash, tx);
//    ledger.change_latest (transaction, account, hash, /*info.rep_block,*/ new_balance, info.block_count + 1);
////    ledger.store.representation_add (transaction, info.rep_block, pending.amount.number ());
//    ledger.store.frontier_del (transaction, tx.hashables.previous);
//    ledger.store.frontier_put (transaction, hash, account);
//    result.account = account;
//    result.amount = pending.amount;
//    ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::receive);
}

void ledger_processor::open_block (germ::open_block const & tx)
{
//    auto hash (tx.hash ());
//    auto existing (ledger.store.block_exists (transaction, hash));
//    result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block already? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    auto source_missing (!ledger.store.block_exists (transaction, tx.hashables.source));
//    result.code = source_missing ? germ::process_result::gap_source : germ::process_result::progress; // Have we seen the source block? (Harmless)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = germ::validate_message (tx.hashables.account, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is the signature valid (Malformed)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::account_info info;
//    result.code = (!ledger.store.account_get (transaction, tx.hashables.account, info)) ? germ::process_result::progress : germ::process_result::fork; // Has this account already been opened? (Malicious)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::pending_key key (tx.hashables.account, tx.hashables.source);
//    germ::pending_info pending;
//    result.code = (!ledger.store.pending_get (transaction, key, pending)) ? germ::process_result::unreceivable : germ::process_result::progress; // Has this source already been received (Malformed)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    result.code = tx.hashables.account == germ::burn_account ? germ::process_result::opened_burn_account : germ::process_result::progress; // Is it burning 0 account? (Malicious)
//    if (result.code != germ::process_result::progress)
//        return;
//
//    germ::account_info source_info;
//    auto found (ledger.store.account_get (transaction, pending.source, source_info));
//    assert (found);
//    ledger.store.pending_del (transaction, key);
//    ledger.store.block_put (transaction, hash, tx);
//    ledger.change_latest (transaction, tx.hashables.account, hash, /*hash,*/ pending.amount.number (), info.block_count + 1);
////    ledger.store.representation_add (transaction, hash, pending.amount.number ());
//    ledger.store.frontier_put (transaction, hash, tx.hashables.account);
//    result.account = tx.hashables.account;
//    result.amount = pending.amount;
//    ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::open);
}

void ledger_processor::tx(germ::tx const& tx)
{
    germ::block_type tx_type = tx.type();
    if (tx_type == germ::block_type::send)
    {
        auto hash (tx.hash ());
        auto existing (ledger.store.block_exists (transaction, hash));
        result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block before? (Harmless)
        if (result.code != germ::process_result::progress)
            return;

        auto previous (ledger.store.block_get (transaction, tx.previous_));
        result.code = previous != nullptr ? germ::process_result::progress : germ::process_result::gap_previous; // Have we seen the previous block already? (Harmless)
        if (result.code != germ::process_result::progress)
            return;

        result.code = tx.valid_predecessor (*previous) ? germ::process_result::progress : germ::process_result::block_position;
        if (result.code != germ::process_result::progress)
            return;

        auto account (ledger.store.frontier_get (transaction, tx.previous_));
        result.code = account.is_zero () ? germ::process_result::fork : germ::process_result::progress;
        if (result.code != germ::process_result::progress)
            return;

        result.code = validate_message (tx.account_, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is this block signed correctly (Malformed)
        if (result.code != germ::process_result::progress)
            return;

        germ::account_info info;
        auto latest_found (ledger.store.account_get (transaction, account, info));
        assert (latest_found);
        assert (info.head == tx.previous_);
        result.code = info.balance.number () >= tx.balance_.number () ? germ::process_result::progress : germ::process_result::negative_spend; // Is this trying to spend a negative amount (Malicious)
        if (result.code != germ::process_result::progress)
            return;

        auto amount (info.balance.number () - tx.balance_.number ());
//    ledger.store.representation_add (transaction, info.rep_block, 0 - amount);
        ledger.store.block_put (transaction, hash, tx);
        ledger.change_latest (transaction, account, hash, /*info.rep_block,*/ tx.balance_, info.block_count + 1);
        ledger.store.pending_put (transaction, germ::pending_key (tx.destination_, hash), { account, amount });
        ledger.store.frontier_del (transaction, tx.previous_);
        ledger.store.frontier_put (transaction, hash, account);
        result.account = account;
        result.amount = amount;
        result.pending_account = tx.destination_;
        ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::send);
    }
    else if(tx_type == germ::block_type::vote)
    {

    }
    else if(tx_type == germ::block_type::receive)
    {
        auto hash (tx.hash ());
        auto existing (ledger.store.block_exists (transaction, hash));
        result.code = existing ? germ::process_result::old : germ::process_result::progress; // Have we seen this block already?  (Harmless)
        if (result.code != germ::process_result::progress)
            return;

        auto previous (ledger.store.block_get (transaction, tx.previous_));
        if (previous != nullptr)
        {
            result.code = germ::process_result::progress;
        }
        else
        {
            if (tx.previous_ == tx.account_)
            {
                auto source_missing (!ledger.store.block_exists (transaction, tx.source_));
                result.code = source_missing ? germ::process_result::gap_source : germ::process_result::progress; // Have we seen the source block? (Harmless)
                if (result.code != germ::process_result::progress)
                    return;

                result.code = germ::validate_message (tx.account_, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is the signature valid (Malformed)
                if (result.code != germ::process_result::progress)
                    return;

                germ::account_info info;
                result.code = (!ledger.store.account_get (transaction, tx.account_, info)) ? germ::process_result::progress : germ::process_result::fork; // Has this account already been opened? (Malicious)
                if (result.code != germ::process_result::progress)
                    return;

//                auto latest(ledger.latest(transaction, tx.source_));
                germ::pending_key key (tx.account_, tx.source_);
                germ::pending_info pending;
                result.code = (!ledger.store.pending_get (transaction, key, pending)) ? germ::process_result::unreceivable : germ::process_result::progress; // Has this source already been received (Malformed)
                if (result.code != germ::process_result::progress)
                    return;

                result.code = tx.account_ == germ::burn_account ? germ::process_result::opened_burn_account : germ::process_result::progress; // Is it burning 0 account? (Malicious)
                if (result.code != germ::process_result::progress)
                    return;

                germ::account_info source_info;
                auto found (ledger.store.account_get (transaction, pending.source, source_info));
                assert (found);
                ledger.store.pending_del (transaction, key);
                ledger.store.block_put (transaction, hash, tx);
                ledger.change_latest (transaction, tx.account_, hash, pending.amount.number (), info.block_count + 1);
                ledger.store.frontier_put (transaction, hash, tx.account_);
                result.account = tx.account_;
                result.amount = pending.amount;
                ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::receive);
                return;
            } else
            {
                result.code = germ::process_result::gap_previous;
            }
        }
        if (result.code != germ::process_result::progress)
            return;


        result.code = tx.valid_predecessor (*previous) ? germ::process_result::progress : germ::process_result::block_position;
        if (result.code != germ::process_result::progress)
            return;

//        auto latest (ledger.latest (transaction, tx.source_));
        result.code = ledger.store.block_exists (transaction, tx.source_) ? germ::process_result::progress : germ::process_result::gap_source; // Have we seen the source block already? (Harmless)
        if (result.code != germ::process_result::progress)
            return;

        auto account (ledger.store.frontier_get (transaction, tx.previous_));
        result.code = account.is_zero () ? germ::process_result::gap_previous : germ::process_result::progress; //Have we seen the previous block? No entries for account at all (Harmless)
        if (result.code != germ::process_result::progress)
        {
            result.code = ledger.store.block_exists (transaction, tx.previous_) ? germ::process_result::fork : germ::process_result::gap_previous; // If we have the block but it's not the latest we have a signed fork (Malicious)
            return;
        }

        result.code = germ::validate_message (tx.account_, hash, tx.signature) ? germ::process_result::bad_signature : germ::process_result::progress; // Is the signature valid (Malformed)
        if (result.code != germ::process_result::progress)
            return;

        germ::account_info info;
        ledger.store.account_get (transaction, account, info);
        result.code = info.head == tx.previous_ ? germ::process_result::progress : germ::process_result::gap_previous; // Block doesn't immediately follow latest block (Harmless)
        if (result.code != germ::process_result::progress)
            return;

        germ::pending_key key (account, tx.source_);
        germ::pending_info pending;
        result.code = (!ledger.store.pending_get (transaction, key, pending)) ? germ::process_result::unreceivable : germ::process_result::progress; // Has this source already been received (Malformed)
        if (result.code != germ::process_result::progress)
            return;

        auto new_balance (info.balance.number () + pending.amount.number ());
        germ::account_info source_info;
        auto found (ledger.store.account_get (transaction, pending.source, source_info));
        assert (found);
        ledger.store.pending_del (transaction, key);
        ledger.store.block_put (transaction, hash, tx);
        ledger.change_latest (transaction, account, hash, new_balance, info.block_count + 1);
        ledger.store.frontier_del (transaction, tx.previous_);
        ledger.store.frontier_put (transaction, hash, account);
        result.account = account;
        result.amount = pending.amount;
        ledger.stats.inc (germ::stat::type::ledger, germ::stat::detail::receive);

    }
}

ledger_processor::ledger_processor (germ::ledger & ledger_a, MDB_txn * transaction_a) :
ledger (ledger_a),
transaction (transaction_a)
{
}
} // namespace

size_t germ::shared_ptr_block_hash::operator() (std::shared_ptr<germ::tx> const & tx) const
{
    auto hash (tx->hash ());
    auto result (static_cast<size_t> (hash.qwords[0]));
    return result;
}

bool germ::shared_ptr_block_hash::operator() (std::shared_ptr<germ::tx> const & lhs, std::shared_ptr<germ::tx> const & rhs) const
{
    return lhs->hash () == rhs->hash ();
}

germ::ledger::ledger (germ::block_store & store_a, germ::stat & stat_a) :
store (store_a),
stats (stat_a),
check_bootstrap_weights (true)
{
}

// Sum the weights for each vote and return the winning block with its vote tally
std::pair<germ::uint128_t, std::shared_ptr<germ::tx>> germ::ledger::winner (MDB_txn * transaction_a, germ::votes const & votes_a)
{
    auto tally_l (tally (transaction_a, votes_a));
    auto existing (tally_l.begin ());
    return std::make_pair (existing->first, existing->second);
}

std::map<germ::uint128_t, std::shared_ptr<germ::tx>, std::greater<germ::uint128_t>> germ::ledger::tally (MDB_txn * transaction_a, germ::votes const & votes_a)
{
    std::unordered_map<std::shared_ptr<germ::tx>, germ::uint128_t, germ::shared_ptr_block_hash, germ::shared_ptr_block_hash> totals;
    // Construct a map of blocks -> vote total.
    for (auto & i : votes_a.rep_votes)
    {
        auto existing (totals.find (i.second));
        if (existing == totals.end ())
        {
            totals.insert (std::make_pair (i.second, 0));
            existing = totals.find (i.second);
            assert (existing != totals.end ());
        }
        auto weight_l (weight (transaction_a, i.first));
        existing->second += weight_l;
    }
    // Construction a map of vote total -> block in decreasing order.
    std::map<germ::uint128_t, std::shared_ptr<germ::tx>, std::greater<germ::uint128_t>> result;
    for (auto & i : totals)
    {
        result[i.second] = i.first;
    }
    return result;
}

// Balance for account containing hash
germ::uint128_t germ::ledger::balance (MDB_txn * transaction_a, germ::block_hash const & hash_a)
{
    balance_visitor visitor (transaction_a, store);
    visitor.compute (hash_a);
    return visitor.balance;
}

// Balance for an account by account number
germ::uint128_t germ::ledger::account_balance (MDB_txn * transaction_a, germ::account const & account_a)
{
    germ::uint128_t result (0);
    germ::account_info info;
    auto found (store.account_get (transaction_a, account_a, info));
    if (found)
    {
        result = info.balance.number ();
    }
    return result;
}

germ::uint128_t germ::ledger::account_pending (MDB_txn * transaction_a, germ::account const & account_a)
{
    germ::uint128_t result (0);
    germ::account end (account_a.number () + 1);
    for (auto i (store.pending_begin (transaction_a, germ::pending_key (account_a, 0))), n (store.pending_begin (transaction_a, germ::pending_key (end, 0))); i != n; ++i)
    {
        germ::pending_info info (i->second);
        result += info.amount.number ();
    }
    return result;
}

germ::process_return germ::ledger::process (MDB_txn * transaction_a, germ::tx const & tx)
{
    ledger_processor processor (*this, transaction_a);
    tx.visit (processor);
    return processor.result;
}

//germ::block_hash germ::ledger::representative (MDB_txn * transaction_a, germ::block_hash const & hash_a)
//{
//    auto result (representative_calculated (transaction_a, hash_a));
//    assert (result.is_zero () || store.block_exists (transaction_a, result));
//    return result;
//}
//
//germ::block_hash germ::ledger::representative_calculated (MDB_txn * transaction_a, germ::block_hash const & hash_a)
//{
//    representative_visitor visitor (transaction_a, store);
//    visitor.compute (hash_a);
//    return visitor.result;
//}

bool germ::ledger::block_exists (germ::block_hash const & hash_a)
{
    germ::transaction transaction (store.environment, nullptr, false);
    auto result (store.block_exists (transaction, hash_a));
    return result;
}

std::string germ::ledger::block_text (char const * hash_a)
{
    return block_text (germ::block_hash (hash_a));
}

std::string germ::ledger::block_text (germ::block_hash const & hash_a)
{
    std::string result;
    germ::transaction transaction (store.environment, nullptr, false);
    auto block (store.block_get (transaction, hash_a));
    if (block != nullptr)
    {
        block->serialize_json (result);
    }
    return result;
}

bool germ::ledger::is_send (MDB_txn * transaction_a, germ::tx const & tx)
{
    bool result (false);
    germ::block_hash previous (tx.previous_);
    if (!previous.is_zero ())
    {
        if (tx.balance_ < balance (transaction_a, previous))
        {
            result = true;
        }
    }
    return result;
}

germ::block_hash germ::ledger::block_destination (MDB_txn * transaction_a, germ::tx const & tx)
{
    germ::block_hash result (0);
//    germ::send_block const * send_block (dynamic_cast<germ::send_block const *> (&tx));
//    germ::state_block const * state_block (dynamic_cast<germ::state_block const *> (&tx));
//    if (send_block != nullptr)
//    {
//        result = send_block->hashables.destination;
//    }
//    else if (state_block != nullptr && is_send (transaction_a, *state_block))
//    {
//        result = state_block->hashables.link;
//    }

    result = tx.destination_;

    return result;
}

germ::block_hash germ::ledger::block_source (MDB_txn * transaction_a, germ::tx const & tx)
{
    // If tx.source () is nonzero, then we have our source.
    // However, universal blocks will always return zero.
    germ::block_hash result (tx.source ());
//    germ::state_block const * state_block (dynamic_cast<germ::state_block const *> (&tx));
//    if (state_block != nullptr && !is_send (transaction_a, *state_block))
//    {
//        result = state_block->hashables.link;
//    }
    return result;
}

// Vote weight of an account
germ::uint128_t germ::ledger::weight (MDB_txn * transaction_a, germ::account const & account_a)
{
    if (check_bootstrap_weights.load ())
    {
        auto blocks = store.block_count (transaction_a);
        if (blocks.sum () < bootstrap_weight_max_blocks)
        {
            auto weight = bootstrap_weights.find (account_a);
            if (weight != bootstrap_weights.end ())
            {
                return weight->second;
            }
        }
        else
        {
            check_bootstrap_weights = false;
        }
    }
//    return store.representation_get (transaction_a, account_a);
    return 0;
}

// Rollback blocks until `tx' doesn't exist
void germ::ledger::rollback (MDB_txn * transaction_a, germ::block_hash const & tx)
{
    assert (store.block_exists (transaction_a, tx));
    auto account_l (account (transaction_a, tx));
    rollback_visitor rollback (transaction_a, *this);
    germ::account_info info;
    while (store.block_exists (transaction_a, tx))
    {
        auto latest_found (store.account_get (transaction_a, account_l, info));
        assert (latest_found);
        auto block (store.block_get (transaction_a, info.head));
        block->visit (rollback);
    }
}

// Return account containing hash
germ::account germ::ledger::account (MDB_txn * transaction_a, germ::block_hash const & hash_a)
{
    germ::account result;
    auto hash (hash_a);
    germ::block_hash successor (1);
    germ::block_info block_info;
    std::unique_ptr<germ::tx> block (store.block_get (transaction_a, hash));
    while (!successor.is_zero () && block->type () != germ::block_type::state && !store.block_info_get (transaction_a, successor, block_info))
    {
        successor = store.block_successor (transaction_a, hash);
        if (!successor.is_zero ())
        {
            hash = successor;
            block = store.block_get (transaction_a, hash);
        }
    }
    if (block->type () == germ::block_type::state)
    {
//        auto state_block (dynamic_cast<germ::state_block *> (block.get ()));
//        result = state_block->hashables.account;
        result = block.get()->destination_;
    }
    else if (successor.is_zero ())
    {
        result = store.frontier_get (transaction_a, hash);
    }
    else
    {
        result = block_info.account;
    }
    assert (!result.is_zero ());
    return result;
}

// Return amount decrease or increase for block
germ::uint128_t germ::ledger::amount (MDB_txn * transaction_a, germ::block_hash const & hash_a)
{
    amount_visitor amount (transaction_a, store);
    amount.compute (hash_a);
    return amount.amount;
}

// Return latest block for account
germ::block_hash germ::ledger::latest (MDB_txn * transaction_a, germ::account const & account_a)
{
    germ::account_info info;
    auto latest_found (store.account_get (transaction_a, account_a, info));
    return latest_found ? info.head : 0;
}

// Return latest root for account, account number of there are no blocks for this account.
germ::block_hash germ::ledger::latest_root (MDB_txn * transaction_a, germ::account const & account_a)
{
    germ::account_info info;
    auto latest_found (store.account_get (transaction_a, account_a, info));
    germ::block_hash result;
    if (!latest_found)
    {
        result = account_a;
    }
    else
    {
        result = info.head;
    }
    return result;
}

germ::checksum germ::ledger::checksum (MDB_txn * transaction_a, germ::account const & begin_a, germ::account const & end_a)
{
    germ::checksum result;
    auto found (store.checksum_get (transaction_a, 0, 0, result));
    assert (found);
    return result;
}

void germ::ledger::dump_account_chain (germ::account const & account_a)
{
    germ::transaction transaction (store.environment, nullptr, false);
    auto hash (latest (transaction, account_a));
    while (!hash.is_zero ())
    {
        auto block (store.block_get (transaction, hash));
        assert (block != nullptr);
        std::cerr << hash.to_string () << std::endl;
        hash = block->previous ();
    }
}

void germ::ledger::checksum_update (MDB_txn * transaction_a, germ::block_hash const & hash_a)
{
    germ::checksum value;
    auto found (store.checksum_get (transaction_a, 0, 0, value));
    assert (found);
    value ^= hash_a;
    store.checksum_put (transaction_a, 0, 0, value);
}

void germ::ledger::change_latest (MDB_txn * transaction_a, germ::account const & account_a, germ::block_hash const & hash_a, /*germ::block_hash const & rep_tx,*/ germ::amount const & balance_a, uint64_t block_count_a, bool is_state)
{
    germ::account_info info;
    auto exists (store.account_get (transaction_a, account_a, info));
    if (exists)
    {
        checksum_update (transaction_a, info.head);
    }
    else
    {
//        assert (store.block_get (transaction_a, hash_a)->previous ().is_zero ());
        info.open_block = hash_a;
    }
    if (!hash_a.is_zero ())
    {
        info.head = hash_a;
//        info.rep_block = rep_tx;
        info.balance = balance_a;
        info.modified = germ::seconds_since_epoch ();
        info.block_count = block_count_a;
        store.account_put (transaction_a, account_a, info);
        if (!(block_count_a % store.block_info_max) && !is_state)
        {
            germ::block_info block_info;
            block_info.account = account_a;
            block_info.balance = balance_a;
            store.block_info_put (transaction_a, hash_a, block_info);
        }
        checksum_update (transaction_a, hash_a);
    }
    else
    {
        store.account_del (transaction_a, account_a);
    }
}

std::unique_ptr<germ::tx> germ::ledger::successor (MDB_txn * transaction_a, germ::uint256_union const & root_a)
{
    germ::block_hash successor (0);
    if (store.account_exists (transaction_a, root_a))
    {
        germ::account_info info;
        auto found (store.account_get (transaction_a, root_a, info));
        assert (found);
        successor = info.open_block;
    }
    else
    {
        successor = store.block_successor (transaction_a, root_a);
    }
    std::unique_ptr<germ::tx> result;
    if (!successor.is_zero ())
    {
        result = store.block_get (transaction_a, successor);
    }
    assert (successor.is_zero () || result != nullptr);
    return result;
}

std::unique_ptr<germ::tx> germ::ledger::forked_block (MDB_txn * transaction_a, germ::tx const & tx)
{
    assert (!store.block_exists (transaction_a, tx.hash ()));
    auto root (tx.root ());
    assert (store.block_exists (transaction_a, root) || store.account_exists (transaction_a, root));
    std::unique_ptr<germ::tx> result (store.block_get (transaction_a, store.block_successor (transaction_a, root)));
    if (result == nullptr)
    {
        germ::account_info info;
        auto found (store.account_get (transaction_a, root, info));
        assert (found);
        result = store.block_get (transaction_a, info.open_block);
        assert (result != nullptr);
    }
    return result;
}
