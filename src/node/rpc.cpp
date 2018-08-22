#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <src/node/rpc.hpp>

#include <src/lib/interface.h>
#include <src/node/node.hpp>

#include <ed25519-donna/ed25519.h>
#include <src/lib/tx.h>

#ifdef GERMBLOCKS_SECURE_RPC
#include <src/node/rpc_secure.hpp>
#endif

germ::rpc_secure_config::rpc_secure_config () :
enable (false),
verbose_logging (false)
{
}

void germ::rpc_secure_config::serialize_json (boost::property_tree::ptree & tree_a) const
{
    tree_a.put ("enable", enable);
    tree_a.put ("verbose_logging", verbose_logging);
    tree_a.put ("server_key_passphrase", server_key_passphrase);
    tree_a.put ("server_cert_path", server_cert_path);
    tree_a.put ("server_key_path", server_key_path);
    tree_a.put ("server_dh_path", server_dh_path);
    tree_a.put ("client_certs_path", client_certs_path);
}

bool germ::rpc_secure_config::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto error (false);
    try
    {
        enable = tree_a.get<bool> ("enable");
        verbose_logging = tree_a.get<bool> ("verbose_logging");
        server_key_passphrase = tree_a.get<std::string> ("server_key_passphrase");
        server_cert_path = tree_a.get<std::string> ("server_cert_path");
        server_key_path = tree_a.get<std::string> ("server_key_path");
        server_dh_path = tree_a.get<std::string> ("server_dh_path");
        client_certs_path = tree_a.get<std::string> ("client_certs_path");
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
    return error;
}

germ::rpc_config::rpc_config () :
address (boost::asio::ip::address_v6::loopback ()),
port (germ::rpc::rpc_port),
enable_control (false),
frontier_request_limit (16384),
chain_request_limit (16384)
{
}

germ::rpc_config::rpc_config (bool enable_control_a) :
address (boost::asio::ip::address_v6::loopback ()),
port (germ::rpc::rpc_port),
enable_control (enable_control_a),
frontier_request_limit (16384),
chain_request_limit (16384)
{
}

void germ::rpc_config::serialize_json (boost::property_tree::ptree & tree_a) const
{
    tree_a.put ("address", address.to_string ());
    tree_a.put ("port", std::to_string (port));
    tree_a.put ("enable_control", enable_control);
    tree_a.put ("frontier_request_limit", frontier_request_limit);
    tree_a.put ("chain_request_limit", chain_request_limit);
}

bool germ::rpc_config::deserialize_json (boost::property_tree::ptree const & tree_a)
{
    auto result (false);
    try
    {
        auto rpc_secure_l (tree_a.get_child_optional ("secure"));
        if (rpc_secure_l)
        {
            result = secure.deserialize_json (rpc_secure_l.get ());
        }

        if (result)
            return result;

        auto address_l (tree_a.get<std::string> ("address"));
        auto port_l (tree_a.get<std::string> ("port"));
        enable_control = tree_a.get<bool> ("enable_control");
        auto frontier_request_limit_l (tree_a.get<std::string> ("frontier_request_limit"));
        auto chain_request_limit_l (tree_a.get<std::string> ("chain_request_limit"));
        try
        {
            port = std::stoul (port_l);
            result = port > std::numeric_limits<uint16_t>::max ();
            frontier_request_limit = std::stoull (frontier_request_limit_l);
            chain_request_limit = std::stoull (chain_request_limit_l);
        }
        catch (std::logic_error const &)
        {
            result = true;
        }
        boost::system::error_code ec;
        address = boost::asio::ip::address_v6::from_string (address_l, ec);
        if (ec)
        {
            result = true;
        }
    }
    catch (std::runtime_error const &)
    {
        result = true;
    }
    return result;
}

germ::rpc::rpc (boost::asio::io_service & service_a, germ::node & node_a, germ::rpc_config const & config_a) :
acceptor (service_a),
config (config_a),
node (node_a)
{
}

void germ::rpc::start ()
{
    auto endpoint (germ::tcp_endpoint (config.address, config.port));
    acceptor.open (endpoint.protocol ());
    acceptor.set_option (boost::asio::ip::tcp::acceptor::reuse_address (true));

    boost::system::error_code ec;
    acceptor.bind (endpoint, ec);
    if (ec)
    {
        BOOST_LOG (node.log) << boost::str (boost::format ("Error while binding for RPC on port %1%: %2%") % endpoint.port () % ec.message ());
        throw std::runtime_error (ec.message ());
    }

    acceptor.listen ();
    node.observers.blocks.add ([this](std::shared_ptr<germ::tx> block_a, germ::account const & account_a, germ::uint128_t const &, bool) {
        observer_action (account_a);
    });

    accept ();
}

void germ::rpc::accept ()
{
    auto connection (std::make_shared<germ::rpc_connection> (node, *this));
    acceptor.async_accept (connection->socket, [this, connection](boost::system::error_code const & ec) {
        if (!ec)
        {
            accept ();
            connection->parse_connection ();
        }
        else
        {
            BOOST_LOG (this->node.log) << boost::str (boost::format ("Error accepting RPC connections: %1%") % ec);
        }
    });
}

void germ::rpc::stop ()
{
    acceptor.close ();
}

germ::rpc_handler::rpc_handler (germ::node & node_a, germ::rpc & rpc_a, std::string const & body_a, std::function<void(boost::property_tree::ptree const &)> const & response_a) :
body (body_a),
node (node_a),
rpc (rpc_a),
response (response_a)
{
}

void germ::rpc::observer_action (germ::account const & account_a)
{
    std::shared_ptr<germ::payment_observer> observer;
    {
        std::lock_guard<std::mutex> lock (mutex);
        auto existing (payment_observers.find (account_a));
        if (existing != payment_observers.end ())
        {
            observer = existing->second;
        }
    }
    if (observer != nullptr)
    {
        observer->observe ();
    }
}

void germ::error_response (std::function<void(boost::property_tree::ptree const &)> response_a, std::string const & message_a)
{
    boost::property_tree::ptree response_l;
    response_l.put ("error", message_a);
    response_a (response_l);
}

namespace
{
bool decode_unsigned (std::string const & text, uint64_t & number)
{
    bool result;
    size_t end;
    try
    {
        number = std::stoull (text, &end);
        result = false;
    }
    catch (std::invalid_argument const &)
    {
        result = true;
    }
    catch (std::out_of_range const &)
    {
        result = true;
    }
    result = result || end != text.size ();
    return result;
}
}

void germ::rpc_handler::account_balance ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union account;
    auto error (account.decode_account (account_text));
    if (!error)
    {
        auto balance (node.balance_pending (account));
        boost::property_tree::ptree response_l;
        response_l.put ("balance", balance.first.convert_to<std::string> ());
        response_l.put ("pending", balance.second.convert_to<std::string> ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad account number");
    }
}

void germ::rpc_handler::account_block_count ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union account;
    auto error (account.decode_account (account_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    germ::account_info info;
    if (!node.store.account_get (transaction, account, info))
    {
        error_response (response, "Account not found");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("block_count", std::to_string (info.block_count));
    response (response_l);
}

void germ::rpc_handler::account_create ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    const bool generate_work = request.get<bool> ("work", true);
    germ::account new_key (existing->second->deterministic_insert (generate_work));
    if (new_key.is_zero ())
    {
        error_response (response, "Wallet is locked");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("account", new_key.to_account ());
    response (response_l);
}

void germ::rpc_handler::account_get ()
{
    std::string key_text (request.get<std::string> ("key"));
    germ::uint256_union pub;
    auto error (pub.decode_hex (key_text));
    if (!error)
    {
        boost::property_tree::ptree response_l;
        response_l.put ("account", pub.to_account ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad public key");
    }
}

void germ::rpc_handler::account_info ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union account;
    auto error (account.decode_account (account_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

//    const bool representative = request.get<bool> ("representative", false);
    const bool weight = request.get<bool> ("weight", false);
    const bool pending = request.get<bool> ("pending", false);
    germ::transaction transaction (node.store.environment, nullptr, false);
    germ::account_info info;
    if (!node.store.account_get (transaction, account, info))
    {
        error_response (response, "Account not found");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("frontier", info.head.to_string ());
    response_l.put ("open_block", info.open_block.to_string ());
//    response_l.put ("representative_block", info.rep_block.to_string ());
    std::string balance;
    germ::uint128_union (info.balance).encode_dec (balance);
    response_l.put ("balance", balance);
    response_l.put ("modified_timestamp", std::to_string (info.modified));
    response_l.put ("block_count", std::to_string (info.block_count));
    if (weight)
    {
        auto account_weight (node.ledger.weight (transaction, account));
        response_l.put ("weight", account_weight.convert_to<std::string> ());
    }
    if (pending)
    {
        auto account_pending (node.ledger.account_pending (transaction, account));
        response_l.put ("pending", account_pending.convert_to<std::string> ());
    }
    response (response_l);
}

void germ::rpc_handler::account_key ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error (account.decode_account (account_text));
    if (!error)
    {
        boost::property_tree::ptree response_l;
        response_l.put ("key", account.to_string ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad account number");
    }
}

void germ::rpc_handler::account_list ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree accounts;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), j (existing->second->store.end ()); i != j; ++i)
    {
        boost::property_tree::ptree entry;
        entry.put ("", germ::uint256_union (i->first.uint256 ()).to_account ());
        accounts.push_back (std::make_pair ("", entry));
    }
    response_l.add_child ("accounts", accounts);
    response (response_l);
}

void germ::rpc_handler::account_move ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    std::string source_text (request.get<std::string> ("source"));
    auto accounts_text (request.get_child ("accounts"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    auto wallet_acc (existing->second);
    germ::uint256_union source;
    auto error_src (source.decode_hex (source_text));
    if (error_src)
    {
        error_response (response, "Bad source number");
        return;
    }

    auto existing_src (node.wallets.items.find (source));
    if (existing_src == node.wallets.items.end ())
    {
        error_response (response, "Source not found");
        return;
    }

    auto source_acc (existing->second);
    std::vector<germ::public_key> accounts;
    for (auto i (accounts_text.begin ()), n (accounts_text.end ()); i != n; ++i)
    {
        germ::public_key account;
        account.decode_hex (i->second.get<std::string> (""));
        accounts.push_back (account);
    }
    germ::transaction transaction (node.store.environment, nullptr, true);
    auto error_moved (wallet_acc->store.move (transaction, source_acc->store, accounts));
    boost::property_tree::ptree response_l;
    response_l.put ("moved", error_moved ? "0" : "1");
    response (response_l);
}

void germ::rpc_handler::account_remove ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    auto wallet_acc (existing->second);
    germ::transaction transaction (node.store.environment, nullptr, true);
    if (!existing->second->store.valid_password (transaction))
    {
        error_response (response, "Wallet locked");
        return;
    }

    germ::account account_id;
    auto error_acc (account_id.decode_account (account_text));
    if (error_acc)
    {
        error_response (response, "Bad account number");
        return;
    }

    auto account (wallet_acc->store.find (transaction, account_id));
    if (account == wallet_acc->store.end ())
    {
        error_response (response, "Account not found in wallet");
        return;
    }

    wallet_acc->store.erase (transaction, account_id);
    boost::property_tree::ptree response_l;
    response_l.put ("removed", "1");
    response (response_l);
}

void germ::rpc_handler::account_representative ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error (account.decode_account (account_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    germ::account_info info;
    auto found (node.store.account_get (transaction, account, info));
    if (!found)
    {
        error_response (response, "Account not found");
        return;
    }

//    auto block (node.store.block_get (transaction, info.rep_block));
//    assert (block != nullptr);
    boost::property_tree::ptree response_l;
//    response_l.put ("representative", block->representative ().to_account ());
    response (response_l);
}

void germ::rpc_handler::account_representative_set ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
        return;

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
        return;

    auto wallet_acc (existing->second);
    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error_acc (account.decode_account (account_text));
    if (error_acc)
    {
        error_response (response, "Bad account number");
        return;
    }

//    std::string representative_text (request.get<std::string> ("representative"));
//    germ::account representative;
//    auto error_rep (representative.decode_account (representative_text));
//    if (error_rep)
//        return;

    uint64_t work (0);
    boost::optional<std::string> work_text (request.get_optional<std::string> ("work"));
    if (work_text.is_initialized ())
    {
        auto work_error (germ::from_string_hex (work_text.get (), work));
        if (work_error)
        {
            error_response (response, "Bad work");
        }
    }
    if (work)
    {
        germ::transaction transaction (node.store.environment, nullptr, true);
        germ::account_info info;
        if (node.store.account_get (transaction, account, info))
        {
            if (germ::work_validate (info.head, work))
            {
                existing->second->store.work_put (transaction, account, work);
            }
            else
            {
                error_response (response, "Invalid work");
            }
        }
        else
        {
            error_response (response, "Account not found");
        }
    }
//    auto response_a (response);
//    wallet_acc->change_async (account, representative, [response_a](std::shared_ptr<germ::tx> block) {
//                              germ::block_hash hash (0);
//                              if (block != nullptr)
//                              {
//                                  hash = block->hash ();
//                              }
//                              boost::property_tree::ptree response_l;
//                              response_l.put ("block", hash.to_string ());
//                              response_a (response_l);
//                          },
//                          work == 0);
}

void germ::rpc_handler::account_weight ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union account;
    auto error (account.decode_account (account_text));
    if (!error)
    {
        auto balance (node.weight (account));
        boost::property_tree::ptree response_l;
        response_l.put ("weight", balance.convert_to<std::string> ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad account number");
    }
}

void germ::rpc_handler::accounts_balances ()
{
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree balances;
    for (auto & accounts : request.get_child ("accounts"))
    {
        std::string account_text = accounts.second.data ();
        germ::uint256_union account;
        auto error (account.decode_account (account_text));
        if (!error)
        {
            boost::property_tree::ptree entry;
            auto balance (node.balance_pending (account));
            entry.put ("balance", balance.first.convert_to<std::string> ());
            entry.put ("pending", balance.second.convert_to<std::string> ());
            balances.push_back (std::make_pair (account.to_account (), entry));
        }
        else
        {
            error_response (response, "Bad account number");
        }
    }
    response_l.add_child ("balances", balances);
    response (response_l);
}

void germ::rpc_handler::accounts_create ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    uint64_t count;
    std::string count_text (request.get<std::string> ("count"));
    auto count_error (decode_unsigned (count_text, count));
    if (count_error || count == 0)
    {
        error_response (response, "Invalid count limit");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    const bool generate_work = request.get<bool> ("work", false);
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree accounts;
    for (auto i (0); accounts.size () < count; ++i)
    {
        germ::account new_key (existing->second->deterministic_insert (generate_work));
        if (!new_key.is_zero ())
        {
            boost::property_tree::ptree entry;
            entry.put ("", new_key.to_account ());
            accounts.push_back (std::make_pair ("", entry));
        }
    }
    response_l.add_child ("accounts", accounts);
    response (response_l);
}

void germ::rpc_handler::accounts_frontiers ()
{
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree frontiers;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto & accounts : request.get_child ("accounts"))
    {
        std::string account_text = accounts.second.data ();
        germ::uint256_union account;
        auto error (account.decode_account (account_text));
        if (!error)
        {
            auto latest (node.ledger.latest (transaction, account));
            if (!latest.is_zero ())
            {
                frontiers.put (account.to_account (), latest.to_string ());
            }
        }
        else
        {
            error_response (response, "Bad account number");
        }
    }
    response_l.add_child ("frontiers", frontiers);
    response (response_l);
}

void germ::rpc_handler::accounts_pending ()
{
    uint64_t count (std::numeric_limits<uint64_t>::max ());
    germ::uint128_union threshold (0);
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    boost::optional<std::string> threshold_text (request.get_optional<std::string> ("threshold"));
    if (threshold_text.is_initialized ())
    {
        auto error_threshold (threshold.decode_dec (threshold_text.get ()));
        if (error_threshold)
        {
            error_response (response, "Bad threshold number");
        }
    }
    const bool source = request.get<bool> ("source", false);
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree pending;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto & accounts : request.get_child ("accounts"))
    {
        std::string account_text = accounts.second.data ();
        germ::uint256_union account;
        if (account.decode_account (account_text))
        {
            error_response (response, "Bad account number");
            continue;
        }

        boost::property_tree::ptree peers_l;
        germ::account end (account.number () + 1);
        for (auto i (node.store.pending_begin (transaction, germ::pending_key (account, 0))), n (node.store.pending_begin (transaction, germ::pending_key (end, 0))); i != n && peers_l.size () < count; ++i)
        {
            germ::pending_key key (i->first);
            if (threshold.is_zero () && !source)
            {
                boost::property_tree::ptree entry;
                entry.put ("", key.hash.to_string ());
                peers_l.push_back (std::make_pair ("", entry));
            }
            else
            {
                germ::pending_info info (i->second);
                if (info.amount.number () >= threshold.number ())
                {
                    if (source)
                    {
                        boost::property_tree::ptree pending_tree;
                        pending_tree.put ("amount", info.amount.number ().convert_to<std::string> ());
                        pending_tree.put ("source", info.source.to_account ());
                        peers_l.add_child (key.hash.to_string (), pending_tree);
                    }
                    else
                    {
                        peers_l.put (key.hash.to_string (), info.amount.number ().convert_to<std::string> ());
                    }
                }
            }
        }
        pending.add_child (account.to_account (), peers_l);
    }
    response_l.add_child ("blocks", pending);
    response (response_l);
}

void germ::rpc_handler::available_supply ()
{
    auto genesis_balance (node.balance (germ::genesis_account)); // Cold storage genesis
    auto landing_balance (node.balance (germ::account ("059F68AAB29DE0D3A27443625C7EA9CDDB6517A8B76FE37727EF6A4D76832AD5"))); // Active unavailable account
    auto faucet_balance (node.balance (germ::account ("8E319CE6F3025E5B2DF66DA7AB1467FE48F1679C13DD43BFDB29FA2E9FC40D3B"))); // Faucet account
    auto burned_balance ((node.balance_pending (germ::account (0))).second); // Burning 0 account
    auto available (germ::genesis_amount - genesis_balance - landing_balance - faucet_balance - burned_balance);
    boost::property_tree::ptree response_l;
    response_l.put ("available", available.convert_to<std::string> ());
    response (response_l);
}

void germ::rpc_handler::block ()
{
    std::string hash_text (request.get<std::string> ("hash"));
    germ::uint256_union hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad hash number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto block (node.store.block_get (transaction, hash));
    if (block == nullptr)
    {
        error_response (response, "Block not found");
        return;
    }

    boost::property_tree::ptree response_l;
    std::string contents;
    block->serialize_json (contents);
    response_l.put ("contents", contents);
    response (response_l);
}

void germ::rpc_handler::block_confirm ()
{
    std::string hash_text (request.get<std::string> ("hash"));
    germ::block_hash hash_l;
    if (hash_l.decode_hex (hash_text))
    {
        error_response (response, "Invalid block hash");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto block_l (node.store.block_get (transaction, hash_l));
    if (block_l == nullptr)
    {
        error_response (response, "Block not found");
        return;
    }

    node.block_confirm (std::move (block_l));
    boost::property_tree::ptree response_l;
    response_l.put ("started", "1");
    response (response_l);
}

void germ::rpc_handler::blocks ()
{
    std::vector<std::string> hashes;
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree blocks;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (boost::property_tree::ptree::value_type & hashes : request.get_child ("hashes"))
    {
        std::string hash_text = hashes.second.data ();
        germ::uint256_union hash;
        auto error (hash.decode_hex (hash_text));
        if (error)
        {
            error_response (response, "Bad hash number");
            continue;
        }

        auto block (node.store.block_get (transaction, hash));
        if (block == nullptr)
        {
            error_response (response, "Block not found");
            continue;
        }

        std::string contents;
        block->serialize_json (contents);
        blocks.put (hash_text, contents);
    }
    response_l.add_child ("blocks", blocks);
    response (response_l);
}

void germ::rpc_handler::blocks_info ()
{
    const bool pending = request.get<bool> ("pending", false);
    const bool source = request.get<bool> ("source", false);
    const bool balance = request.get<bool> ("balance", false);
    std::vector<std::string> hashes;
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree blocks;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (boost::property_tree::ptree::value_type & hashes : request.get_child ("hashes"))
    {
        std::string hash_text = hashes.second.data ();
        germ::uint256_union hash;
        auto error (hash.decode_hex (hash_text));
        if (error)
        {
            error_response (response, "Bad hash number");
            continue;
        }

        auto block (node.store.block_get (transaction, hash));
        if (block == nullptr)
        {
            error_response (response, "Block not found");
            continue;
        }

        boost::property_tree::ptree entry;
        auto account (node.ledger.account (transaction, hash));
        entry.put ("block_account", account.to_account ());
        auto amount (node.ledger.amount (transaction, hash));
        entry.put ("amount", amount.convert_to<std::string> ());
        std::string contents;
        block->serialize_json (contents);
        entry.put ("contents", contents);
        if (pending)
        {
            bool exists (false);
            auto destination (node.ledger.block_destination (transaction, *block));
            if (!destination.is_zero ())
            {
                exists = node.store.pending_exists (transaction, germ::pending_key (destination, hash));
            }
            entry.put ("pending", exists ? "1" : "0");
        }
        if (source)
        {
            germ::block_hash source_hash (node.ledger.block_source (transaction, *block));
            std::unique_ptr<germ::tx> block_a (node.store.block_get (transaction, source_hash));
            if (block_a != nullptr)
            {
                auto source_account (node.ledger.account (transaction, source_hash));
                entry.put ("source_account", source_account.to_account ());
            }
            else
            {
                entry.put ("source_account", "0");
            }
        }
        if (balance)
        {
            auto balance (node.ledger.balance (transaction, hash));
            entry.put ("balance", balance.convert_to<std::string> ());
        }
        blocks.push_back (std::make_pair (hash_text, entry));
    }
    response_l.add_child ("blocks", blocks);
    response (response_l);
}

void germ::rpc_handler::block_account ()
{
    std::string hash_text (request.get<std::string> ("hash"));
    germ::block_hash hash;
    if (hash.decode_hex (hash_text))
    {
        error_response (response, "Invalid block hash");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    if (!node.store.block_exists (transaction, hash))
    {
        error_response (response, "Block not found");
        return;
    }

    boost::property_tree::ptree response_l;
    auto account (node.ledger.account (transaction, hash));
    response_l.put ("account", account.to_account ());
    response (response_l);
}

void germ::rpc_handler::block_count ()
{
    germ::transaction transaction (node.store.environment, nullptr, false);
    boost::property_tree::ptree response_l;
    response_l.put ("count", std::to_string (node.store.block_count (transaction).sum ()));
    response_l.put ("unchecked", std::to_string (node.store.unchecked_count (transaction)));
    response (response_l);
}

void germ::rpc_handler::block_count_type ()
{
    germ::transaction transaction (node.store.environment, nullptr, false);
    germ::block_counts count (node.store.block_count (transaction));
    boost::property_tree::ptree response_l;
    response_l.put ("send", std::to_string (count.send));
    response_l.put ("receive", std::to_string (count.receive));
    response_l.put ("open", std::to_string (count.open));
    response_l.put ("change", std::to_string (count.change));
    response_l.put ("state", std::to_string (count.state));
    response (response_l);
}

void germ::rpc_handler::block_create ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string type (request.get<std::string> ("type"));
    germ::uint256_union wallet (0);
    boost::optional<std::string> wallet_text (request.get_optional<std::string> ("wallet"));
    if (wallet_text.is_initialized ())
    {
        auto error (wallet.decode_hex (wallet_text.get ()));
        if (error)
        {
            error_response (response, "Bad wallet number");
        }
    }
    germ::uint256_union account (0);
    boost::optional<std::string> account_text (request.get_optional<std::string> ("account"));
    if (account_text.is_initialized ())
    {
        auto error_account (account.decode_account (account_text.get ()));
        if (error_account)
        {
            error_response (response, "Bad account number");
        }
    }
//    germ::uint256_union representative (0);
//    boost::optional<std::string> representative_text (request.get_optional<std::string> ("representative"));
//    if (representative_text.is_initialized ())
//    {
//        auto error_representative (representative.decode_account (representative_text.get ()));
//        if (error_representative)
//        {
//            error_response (response, "Bad representative account");
//        }
//    }
    germ::uint256_union destination (0);
    boost::optional<std::string> destination_text (request.get_optional<std::string> ("destination"));
    if (destination_text.is_initialized ())
    {
        auto error_destination (destination.decode_account (destination_text.get ()));
        if (error_destination)
        {
            error_response (response, "Bad destination account");
        }
    }
    germ::block_hash source (0);
    boost::optional<std::string> source_text (request.get_optional<std::string> ("source"));
    if (source_text.is_initialized ())
    {
        auto error_source (source.decode_hex (source_text.get ()));
        if (error_source)
        {
            error_response (response, "Invalid source hash");
        }
    }
    germ::uint128_union amount (0);
    boost::optional<std::string> amount_text (request.get_optional<std::string> ("amount"));
    if (amount_text.is_initialized ())
    {
        auto error_amount (amount.decode_dec (amount_text.get ()));
        if (error_amount)
        {
            error_response (response, "Bad amount number");
        }
    }
    uint64_t work (0);
    boost::optional<std::string> work_text (request.get_optional<std::string> ("work"));
    if (work_text.is_initialized ())
    {
        auto work_error (germ::from_string_hex (work_text.get (), work));
        if (work_error)
        {
            error_response (response, "Bad work");
        }
    }
    germ::raw_key prv;
    prv.data.clear ();
    germ::uint256_union previous (0);
    germ::uint128_union balance (0);
    if (wallet != 0 && account != 0)
    {
        auto existing (node.wallets.items.find (wallet));
        if (existing != node.wallets.items.end ())
        {
            germ::transaction transaction (node.store.environment, nullptr, false);
            auto unlock_check (existing->second->store.valid_password (transaction));
            if (unlock_check)
            {
                auto account_check (existing->second->store.find (transaction, account));
                if (account_check != existing->second->store.end ())
                {
                    existing->second->store.fetch (transaction, account, prv);
                    previous = node.ledger.latest (transaction, account);
                    balance = node.ledger.account_balance (transaction, account);
                }
                else
                {
                    error_response (response, "Account not found in wallet");
                }
            }
            else
            {
                error_response (response, "Wallet is locked");
            }
        }
        else
        {
            error_response (response, "Wallet not found");
        }
    }
    boost::optional<std::string> key_text (request.get_optional<std::string> ("key"));
    if (key_text.is_initialized ())
    {
        auto error_key (prv.data.decode_hex (key_text.get ()));
        if (error_key)
        {
            error_response (response, "Bad private key");
        }
    }
    boost::optional<std::string> previous_text (request.get_optional<std::string> ("previous"));
    if (previous_text.is_initialized ())
    {
        auto error_previous (previous.decode_hex (previous_text.get ()));
        if (error_previous)
        {
            error_response (response, "Invalid previous hash");
        }
    }
    boost::optional<std::string> balance_text (request.get_optional<std::string> ("balance"));
    if (balance_text.is_initialized ())
    {
        auto error_balance (balance.decode_dec (balance_text.get ()));
        if (error_balance)
        {
            error_response (response, "Bad balance number");
        }
    }
    germ::uint256_union link (0);
    boost::optional<std::string> link_text (request.get_optional<std::string> ("link"));
    if (link_text.is_initialized ())
    {
        auto error_link (link.decode_account (link_text.get ()));
        if (error_link)
        {
            auto error_link (link.decode_hex (link_text.get ()));
            if (error_link)
            {
                error_response (response, "Bad link number");
            }
        }
    }
    else
    {
        // Retrieve link from source or destination
        link = source.is_zero () ? destination : source;
    }
    if (prv.data == 0)
    {
        error_response (response, "Private key or local wallet and account required");
        return;
    }

    germ::uint256_union pub;
    ed25519_publickey (prv.data.bytes.data (), pub.bytes.data ());
    // Fetching account balance & previous for send blocks (if aren't given directly)
    if (!previous_text.is_initialized () && !balance_text.is_initialized ())
    {
        germ::transaction transaction (node.store.environment, nullptr, false);
        previous = node.ledger.latest (transaction, pub);
        balance = node.ledger.account_balance (transaction, pub);
    }
        // Double check current balance if previous block is specified
    else if (previous_text.is_initialized () && balance_text.is_initialized () && type == "send")
    {
        germ::transaction transaction (node.store.environment, nullptr, false);
        if (node.store.block_exists (transaction, previous) && node.store.block_balance (transaction, previous) != balance.number ())
        {
            error_response (response, "Balance mismatch for previous block");
        }
    }
    // Check for incorrect account key
    if (account_text.is_initialized ())
    {
        if (account != pub)
        {
            error_response (response, "Incorrect key for given account");
        }
    }
    if (type == "state")
    {
        if (previous_text.is_initialized () && /*!representative.is_zero () &&*/ (!link.is_zero () || link_text.is_initialized ()))
        {
            if (work == 0)
            {
                work = node.work_generate_blocking (previous.is_zero () ? pub : previous);
            }
            germ::state_block state (pub, previous, /*representative,*/ balance, link, prv, pub, work);
            boost::property_tree::ptree response_l;
            response_l.put ("hash", state.hash ().to_string ());
            std::string contents;
            state.serialize_json (contents);
            response_l.put ("block", contents);
            response (response_l);
        }
        else
        {
//            error_response (response, "Previous, representative, final balance and link (source or destination) are required");
            error_response (response, "Previous, final balance and link (source or destination) are required");
        }
    }
    else if (type == "open")
    {
        if (/*representative == 0 ||*/ source == 0)
        {
            error_response (response, "Representative account and source hash required");
            return;
        }

        if (work == 0)
        {
            work = node.work_generate_blocking (pub);
        }
#if 0
        germ::open_block open (source, representative, pub, prv, pub, work);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", open.hash ().to_string ());
        std::string contents;
        open.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#else
        germ::tx_message tx_msg(1800000, "no open transaction data", 1000, 0);
        germ::tx open(previous, account, source, account, balance,  tx_msg , 0, prv, pub);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", open.hash ().to_string ());
        std::string contents;
        open.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#endif
    }
    else if (type == "receive")
    {
        if (source == 0 || previous == 0)
        {
            error_response (response, "Previous hash and source hash required");
            return;
        }

        if (work == 0)
        {
            work = node.work_generate_blocking (previous);
        }
#if 0
        germ::receive_block receive (previous, source, prv, pub, work);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", receive.hash ().to_string ());
        std::string contents;
        receive.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#else
        germ::tx_message tx_msg(1800000, "no open transaction data", 1000, 0);
        germ::tx receive(previous, account, source, account, balance,  tx_msg , 0, prv, pub);

        boost::property_tree::ptree response_l;
        response_l.put ("hash", receive.hash ().to_string ());
        std::string contents;
        receive.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#endif
    }
    else if (type == "change")
    {
        if (/*representative == 0 ||*/ previous == 0)
        {
            error_response (response, "Representative account and previous hash required");
            return;
        }

        if (work == 0)
        {
            work = node.work_generate_blocking (previous);
        }
        germ::change_block change (previous, /*representative,*/ prv, pub, work);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", change.hash ().to_string ());
        std::string contents;
        change.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
    }
    else if (type == "vote")
    {
        if (/*representative == 0 ||*/ previous == 0)
        {
            error_response (response, "Representative account and previous hash required");
            return;
        }

        if (work == 0)
        {
            work = node.work_generate_blocking (previous);
        }
#if 0
        germ::change_block change (previous, /*representative,*/ prv, pub, work);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", change.hash ().to_string ());
        std::string contents;
        change.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#else
        germ::tx_message tx_msg(1800000, "no open transaction data", 1000, 0);
        germ::tx vote(previous, account, source, account, balance,  tx_msg , 0, prv, pub);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", vote.hash ().to_string ());
        std::string contents;
        vote.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#endif
    }
    else if (type == "send")
    {
        if (destination == 0 || previous == 0 || balance == 0 || amount == 0)
        {
            error_response (response, "Destination account, previous hash, current balance and amount required");
            return;
        }

        if (balance.number () < amount.number ())
        {
            error_response (response, "Insufficient balance");
            return;
        }

        if (work == 0)
        {
            work = node.work_generate_blocking (previous);
        }
#if 0
        germ::send_block send (previous, destination, balance.number () - amount.number (), prv, pub, work);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", send.hash ().to_string ());
        std::string contents;
        send.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#else
        germ::tx_message tx_msg(1800000, "no open transaction data", 1000, 0);
        germ::tx send(previous, account, source, account, balance,  tx_msg , 0, prv, pub);
        boost::property_tree::ptree response_l;
        response_l.put ("hash", send.hash ().to_string ());
        std::string contents;
        send.serialize_json (contents);
        response_l.put ("block", contents);
        response (response_l);
#endif
    }
    else
    {
        error_response (response, "Invalid block type");
    }
}

void germ::rpc_handler::block_hash ()
{
    std::string block_text (request.get<std::string> ("block"));
    boost::property_tree::ptree block_l;
    std::stringstream block_stream (block_text);
    boost::property_tree::read_json (block_stream, block_l);
    block_l.put ("signature", "0");
    block_l.put ("work", "0");
    auto block (germ::deserialize_block_json (block_l));
    if (block != nullptr)
    {
        boost::property_tree::ptree response_l;
        response_l.put ("hash", block->hash ().to_string ());
        response (response_l);
    }
    else
    {
        error_response (response, "Block is invalid");
    }
}

void germ::rpc_handler::successors ()
{
    std::string block_text (request.get<std::string> ("block"));
    std::string count_text (request.get<std::string> ("count"));
    germ::block_hash block;
    if (block.decode_hex (block_text))
    {
        error_response (response, "Invalid block hash");
        return;
    }

    uint64_t count;
    if (decode_unsigned (count_text, count))
    {
        error_response (response, "Invalid count limit");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree blocks;
    germ::transaction transaction (node.store.environment, nullptr, false);
    while (!block.is_zero () && blocks.size () < count)
    {
        auto block_l (node.store.block_get (transaction, block));
        if (block_l != nullptr)
        {
            boost::property_tree::ptree entry;
            entry.put ("", block.to_string ());
            blocks.push_back (std::make_pair ("", entry));
            block = node.store.block_successor (transaction, block);
        }
        else
        {
            block.clear ();
        }
    }
    response_l.add_child ("blocks", blocks);
    response (response_l);
}

void germ::rpc_handler::bootstrap ()
{
    std::string address_text = request.get<std::string> ("address");
    std::string port_text = request.get<std::string> ("port");
    boost::system::error_code ec;
    auto address (boost::asio::ip::address_v6::from_string (address_text, ec));
    if (ec)
    {
        error_response (response, "Invalid address");
        return;
    }

    uint16_t port;
    if (germ::parse_port (port_text, port))
    {
        error_response (response, "Invalid port");
        return;
    }

    node.bootstrap_initiator.bootstrap (germ::endpoint (address, port));
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::bootstrap_any ()
{
    node.bootstrap_initiator.bootstrap ();
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::chain ()
{
    std::string block_text (request.get<std::string> ("block"));
    std::string count_text (request.get<std::string> ("count"));
    germ::block_hash block;
    if (block.decode_hex (block_text))
    {
        error_response (response, "Invalid block hash");
        return;
    }

    uint64_t count;
    if (decode_unsigned (count_text, count))
    {
        error_response (response, "Invalid count limit");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree blocks;
    germ::transaction transaction (node.store.environment, nullptr, false);
    while (!block.is_zero () && blocks.size () < count)
    {
        auto block_l (node.store.block_get (transaction, block));
        if (block_l != nullptr)
        {
            boost::property_tree::ptree entry;
            entry.put ("", block.to_string ());
            blocks.push_back (std::make_pair ("", entry));
            block = block_l->previous ();
        }
        else
        {
            block.clear ();
        }
    }
    response_l.add_child ("blocks", blocks);
    response (response_l);
}

void germ::rpc_handler::confirmation_history ()
{
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree elections;
    {
        std::lock_guard<std::mutex> lock (node.active.mutex);
        for (auto i (node.active.confirmed.begin ()), n (node.active.confirmed.end ()); i != n; ++i)
        {
            boost::property_tree::ptree election;
            election.put ("hash", i->winner->hash ().to_string ());
            election.put ("tally", i->tally.to_string_dec ());
            elections.push_back (std::make_pair ("", election));
        }
    }
    response_l.add_child ("confirmations", elections);
    response (response_l);
}

void germ::rpc_handler::delegators ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error (account.decode_account (account_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree delegators;
    germ::transaction transaction (node.store.environment, nullptr, false);
//    for (auto i (node.store.latest_begin (transaction)), n (node.store.latest_end ()); i != n; ++i)
//    {
//        germ::account_info info (i->second);
//        auto block (node.store.block_get (transaction, info.rep_block));
//        assert (block != nullptr);
//        if (block->representative () == account)
//        {
//            std::string balance;
//            germ::uint128_union (info.balance).encode_dec (balance);
//            delegators.put (germ::account (i->first.uint256 ()).to_account (), balance);
//        }
//    }
    response_l.add_child ("delegators", delegators);
    response (response_l);
}

void germ::rpc_handler::delegators_count ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error (account.decode_account (account_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    uint64_t count (0);
    germ::transaction transaction (node.store.environment, nullptr, false);
//    for (auto i (node.store.latest_begin (transaction)), n (node.store.latest_end ()); i != n; ++i)
//    {
//        germ::account_info info (i->second);
//        auto block (node.store.block_get (transaction, info.rep_block));
//        assert (block != nullptr);
//        if (block->representative () == account)
//        {
//            ++count;
//        }
//    }
    boost::property_tree::ptree response_l;
    response_l.put ("count", std::to_string (count));
    response (response_l);
}

void germ::rpc_handler::deterministic_key ()
{
    std::string seed_text (request.get<std::string> ("seed"));
    std::string index_text (request.get<std::string> ("index"));
    germ::raw_key seed;
    auto error (seed.data.decode_hex (seed_text));
    if (error)
    {
        error_response (response, "Bad seed");
        return;

    }

    uint64_t index_a;
    if (decode_unsigned (index_text, index_a))
    {
        error_response (response, "Invalid index");
        return;
    }

    germ::uint256_union index (index_a);
    germ::uint256_union prv;
    blake2b_state hash;
    blake2b_init (&hash, prv.bytes.size ());
    blake2b_update (&hash, seed.data.bytes.data (), seed.data.bytes.size ());
    blake2b_update (&hash, reinterpret_cast<uint8_t *> (&index.dwords[7]), sizeof (uint32_t));
    blake2b_final (&hash, prv.bytes.data (), prv.bytes.size ());
    boost::property_tree::ptree response_l;
    germ::uint256_union pub;
    ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
    response_l.put ("private", prv.to_string ());
    response_l.put ("public", pub.to_string ());
    response_l.put ("account", pub.to_account ());
    response (response_l);
}

void germ::rpc_handler::frontiers ()
{
    std::string account_text (request.get<std::string> ("account"));
    std::string count_text (request.get<std::string> ("count"));
    germ::account start;
    if (start.decode_account (account_text))
    {
        error_response (response, "Invalid starting account");
        return;
    }

    uint64_t count;
    if (decode_unsigned (count_text, count))
    {
        error_response (response, "Invalid count limit");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree frontiers;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n && frontiers.size () < count; ++i)
    {
        frontiers.put (germ::account (i->first.uint256 ()).to_account (), germ::account_info (i->second).head.to_string ());
    }
    response_l.add_child ("frontiers", frontiers);
    response (response_l);
}

void germ::rpc_handler::account_count ()
{
    germ::transaction transaction (node.store.environment, nullptr, false);
    auto size (node.store.account_count (transaction));
    boost::property_tree::ptree response_l;
    response_l.put ("count", std::to_string (size));
    response (response_l);
}

namespace
{
class history_visitor : public germ::block_visitor
{
public:
    history_visitor (germ::rpc_handler & handler_a, bool raw_a, germ::transaction & transaction_a, boost::property_tree::ptree & tree_a, germ::block_hash const & hash_a) :
    handler (handler_a),
    raw (raw_a),
    transaction (transaction_a),
    tree (tree_a),
    hash (hash_a)
    {
    }
    virtual ~history_visitor () = default;
    void send_block (germ::send_block const & block_a)
    {
        tree.put ("type", "send");
        auto account (block_a.hashables.destination.to_account ());
        tree.put ("account", account);
        auto amount (handler.node.ledger.amount (transaction, hash).convert_to<std::string> ());
        tree.put ("amount", amount);
        if (raw)
        {
            tree.put ("destination", account);
            tree.put ("balance", block_a.hashables.balance.to_string_dec ());
        }
    }
    void receive_block (germ::receive_block const & block_a)
    {
        tree.put ("type", "receive");
        auto account (handler.node.ledger.account (transaction, block_a.hashables.source).to_account ());
        tree.put ("account", account);
        auto amount (handler.node.ledger.amount (transaction, hash).convert_to<std::string> ());
        tree.put ("amount", amount);
        if (raw)
        {
            tree.put ("source", block_a.hashables.source.to_string ());
        }
    }
    void open_block (germ::open_block const & block_a)
    {
        if (raw)
        {
            tree.put ("type", "open");
//            tree.put ("representative", block_a.hashables.representative.to_account ());
            tree.put ("source", block_a.hashables.source.to_string ());
            tree.put ("opened", block_a.hashables.account.to_account ());
        }
        else
        {
            // Report opens as a receive
            tree.put ("type", "receive");
        }
        if (block_a.hashables.source != germ::genesis_account)
        {
            tree.put ("account", handler.node.ledger.account (transaction, block_a.hashables.source).to_account ());
            tree.put ("amount", handler.node.ledger.amount (transaction, hash).convert_to<std::string> ());
        }
        else
        {
            tree.put ("account", germ::genesis_account.to_account ());
            tree.put ("amount", germ::genesis_amount.convert_to<std::string> ());
        }
    }
    void change_block (germ::change_block const & block_a)
    {
        if (raw)
        {
            tree.put ("type", "change");
//            tree.put ("representative", block_a.hashables.representative.to_account ());
        }
    }
    void state_block (germ::state_block const & block_a)
    {
        if (raw)
        {
            tree.put ("type", "state");
//            tree.put ("representative", block_a.hashables.representative.to_account ());
            tree.put ("link", block_a.hashables.link.to_string ());
        }
        auto balance (block_a.hashables.balance.number ());
        auto previous_balance (handler.node.ledger.balance (transaction, block_a.hashables.previous));
        if (balance < previous_balance)
        {
            if (raw)
            {
                tree.put ("subtype", "send");
            }
            else
            {
                tree.put ("type", "send");
            }
            tree.put ("account", block_a.hashables.link.to_account ());
            tree.put ("amount", (previous_balance - balance).convert_to<std::string> ());
        }
        else
        {
            if (block_a.hashables.link.is_zero ())
            {
                if (raw)
                {
                    tree.put ("subtype", "change");
                }
            }
            else
            {
                if (raw)
                {
                    tree.put ("subtype", "receive");
                }
                else
                {
                    tree.put ("type", "receive");
                }
                tree.put ("account", handler.node.ledger.account (transaction, block_a.hashables.link).to_account ());
                tree.put ("amount", (balance - previous_balance).convert_to<std::string> ());
            }
        }
    }
    void tx (germ::tx const & tx) override
    {
        germ::block_type tx_type = tx.type();
        if (tx_type == germ::block_type::send)
        {
            tree.put("type", "send");
        }
        else if (tx_type == germ::block_type::receive)
        {
            tree.put("type", "receive");
        }

        tree.put("previous", tx.previous_.to_string());
        tree.put("destination", tx.destination_.to_account());
        tree.put("source", tx.source_.to_string());
        tree.put("balance", tx.balance_.to_string());

        boost::property_tree::ptree tx_info;
        tx_info.put("value", germ::to_string_hex(tx.tx_info.value));
        tx_info.put("data", tx.tx_info.data);
        tx_info.put("gas", germ::to_string_hex(tx.tx_info.gas));
        tx_info.put("gasprice", germ::to_string_hex(tx.tx_info.gas_price));

        tree.put_child("tx_info", tx_info);

        tree.put("signature", tx.signature.to_string());
    }

    germ::rpc_handler & handler;
    bool raw;
    germ::transaction & transaction;
    boost::property_tree::ptree & tree;
    germ::block_hash const & hash;
};
}

void germ::rpc_handler::account_history ()
{
    std::string account_text;
    std::string count_text (request.get<std::string> ("count"));
    bool output_raw (request.get_optional<bool> ("raw") == true);
    auto error (false);
    germ::block_hash hash;
    auto head_str (request.get_optional<std::string> ("head"));
    germ::transaction transaction (node.store.environment, nullptr, false);
    if (head_str)
    {
        error = hash.decode_hex (*head_str);
        if (!error)
        {
            account_text = node.ledger.account (transaction, hash).to_account ();
        }
        else
        {
            error_response (response, "Invalid block hash");
        }
    }
    else
    {
        account_text = request.get<std::string> ("account");
        germ::uint256_union account;
        error = account.decode_account (account_text);
        if (!error)
        {
            hash = node.ledger.latest (transaction, account);
        }
        else
        {
            error_response (response, "Bad account number");
        }
    }
    if (error)
        return;

    uint64_t count;
    if (decode_unsigned (count_text, count))
    {
        error_response (response, "Invalid count limit");
        return;
    }

    uint64_t offset = 0;
    auto offset_text (request.get_optional<std::string> ("offset"));
    if (offset_text && decode_unsigned (*offset_text, offset))
    {
        error_response (response, "Invalid offset");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree history;
    if (error)
    {
        error_response (response, "Failed to decode head block hash");
        return;
    }

    response_l.put ("account", account_text);
    auto block (node.store.block_get (transaction, hash));
    while (block != nullptr && count > 0)
    {
        if (offset > 0)
        {
            --offset;
        }
        else
        {
            boost::property_tree::ptree entry;
            history_visitor visitor (*this, output_raw, transaction, entry, hash);
            block->visit (visitor);
            if (!entry.empty ())
            {
                entry.put ("hash", hash.to_string ());
                if (output_raw)
                {
//                    entry.put ("work", germ::to_string_hex (block->block_work ()));
                    entry.put ("signature", block->block_signature ().to_string ());
                }
                history.push_back (std::make_pair ("", entry));
            }
            --count;
        }
        hash = block->previous ();
        block = node.store.block_get (transaction, hash);
    }
    response_l.add_child ("history", history);
    if (!hash.is_zero ())
    {
        response_l.put ("previous", hash.to_string ());
    }
    response (response_l);
}

void germ::rpc_handler::keepalive ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string address_text (request.get<std::string> ("address"));
    std::string port_text (request.get<std::string> ("port"));
    uint16_t port;
    if (germ::parse_port (port_text, port))
    {
        error_response (response, "Invalid port");
        return;
    }

    node.keepalive (address_text, port);
    boost::property_tree::ptree response_l;
    response (response_l);
}

void germ::rpc_handler::key_create ()
{
    boost::property_tree::ptree response_l;
    germ::keypair pair;
    response_l.put ("private", pair.prv.data.to_string ());
    response_l.put ("public", pair.pub.to_string ());
    response_l.put ("account", pair.pub.to_account ());
    response (response_l);
}

void germ::rpc_handler::key_expand ()
{
    std::string key_text (request.get<std::string> ("key"));
    germ::uint256_union prv;
    auto error (prv.decode_hex (key_text));
    if (error)
    {
        error_response (response, "Bad private key");
        return;
    }

    boost::property_tree::ptree response_l;
    germ::uint256_union pub;
    ed25519_publickey (prv.bytes.data (), pub.bytes.data ());
    response_l.put ("private", prv.to_string ());
    response_l.put ("public", pub.to_string ());
    response_l.put ("account", pub.to_account ());
    response (response_l);
}

void germ::rpc_handler::ledger ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    germ::account start (0);
    uint64_t count (std::numeric_limits<uint64_t>::max ());
    boost::optional<std::string> account_text (request.get_optional<std::string> ("account"));
    if (account_text.is_initialized ())
    {
        auto error (start.decode_account (account_text.get ()));
        if (error)
        {
            error_response (response, "Invalid starting account");
        }
    }
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error_count (decode_unsigned (count_text.get (), count));
        if (error_count)
        {
            error_response (response, "Invalid count limit");
        }
    }
    uint64_t modified_since (0);
    boost::optional<std::string> modified_since_text (request.get_optional<std::string> ("modified_since"));
    if (modified_since_text.is_initialized ())
    {
        modified_since = strtoul (modified_since_text.get ().c_str (), NULL, 10);
    }
    const bool sorting = request.get<bool> ("sorting", false);
//    const bool representative = request.get<bool> ("representative", false);
    const bool weight = request.get<bool> ("weight", false);
    const bool pending = request.get<bool> ("pending", false);
    boost::property_tree::ptree response_a;
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree accounts;
    germ::transaction transaction (node.store.environment, nullptr, false);
    if (!sorting) // Simple
    {
        for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n && accounts.size () < count; ++i)
        {
            germ::account_info info (i->second);
            if (info.modified >= modified_since)
            {
                germ::account account (i->first.uint256 ());
                boost::property_tree::ptree response_l;
                response_l.put ("frontier", info.head.to_string ());
                response_l.put ("open_block", info.open_block.to_string ());
//                response_l.put ("representative_block", info.rep_block.to_string ());
                std::string balance;
                germ::uint128_union (info.balance).encode_dec (balance);
                response_l.put ("balance", balance);
                response_l.put ("modified_timestamp", std::to_string (info.modified));
                response_l.put ("block_count", std::to_string (info.block_count));
//                if (representative)
//                {
//                    auto block (node.store.block_get (transaction, info.rep_block));
//                    assert (block != nullptr);
//                    response_l.put ("representative", block->representative ().to_account ());
//                }
                if (weight)
                {
                    auto account_weight (node.ledger.weight (transaction, account));
                    response_l.put ("weight", account_weight.convert_to<std::string> ());
                }
                if (pending)
                {
                    auto account_pending (node.ledger.account_pending (transaction, account));
                    response_l.put ("pending", account_pending.convert_to<std::string> ());
                }
                accounts.push_back (std::make_pair (account.to_account (), response_l));
            }
        }
    }
    else // Sorting
    {
        std::vector<std::pair<germ::uint128_union, germ::account>> ledger_l;
        for (auto i (node.store.latest_begin (transaction, start)), n (node.store.latest_end ()); i != n; ++i)
        {
            germ::account_info info (i->second);
            germ::uint128_union balance (info.balance);
            if (info.modified >= modified_since)
            {
                ledger_l.push_back (std::make_pair (balance, germ::account (i->first.uint256 ())));
            }
        }
        std::sort (ledger_l.begin (), ledger_l.end ());
        std::reverse (ledger_l.begin (), ledger_l.end ());
        germ::account_info info;
        for (auto i (ledger_l.begin ()), n (ledger_l.end ()); i != n && accounts.size () < count; ++i)
        {
            node.store.account_get (transaction, i->second, info);
            germ::account account (i->second);
            response_l.put ("frontier", info.head.to_string ());
            response_l.put ("open_block", info.open_block.to_string ());
//            response_l.put ("representative_block", info.rep_block.to_string ());
            std::string balance;
            (i->first).encode_dec (balance);
            response_l.put ("balance", balance);
            response_l.put ("modified_timestamp", std::to_string (info.modified));
            response_l.put ("block_count", std::to_string (info.block_count));
//            if (representative)
//            {
//                auto block (node.store.block_get (transaction, info.rep_block));
//                assert (block != nullptr);
//                response_l.put ("representative", block->representative ().to_account ());
//            }
            if (weight)
            {
                auto account_weight (node.ledger.weight (transaction, account));
                response_l.put ("weight", account_weight.convert_to<std::string> ());
            }
            if (pending)
            {
                auto account_pending (node.ledger.account_pending (transaction, account));
                response_l.put ("pending", account_pending.convert_to<std::string> ());
            }
            accounts.push_back (std::make_pair (account.to_account (), response_l));
        }
    }
    response_a.add_child ("accounts", accounts);
    response (response_a);
}

void germ::rpc_handler::mrai_from_raw ()
{
    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (!amount.decode_dec (amount_text))
    {
        auto result (amount.number () / germ::Mxrb_ratio);
        boost::property_tree::ptree response_l;
        response_l.put ("amount", result.convert_to<std::string> ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad amount number");
    }
}

void germ::rpc_handler::mrai_to_raw ()
{
    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (amount.decode_dec (amount_text))
    {
        error_response (response, "Bad amount number");
        return;
    }

    auto result (amount.number () * germ::Mxrb_ratio);
    if (result <= amount.number ())
    {
        error_response (response, "Amount too big");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("amount", result.convert_to<std::string> ());
    response (response_l);
}

void germ::rpc_handler::krai_from_raw ()
{
    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (!amount.decode_dec (amount_text))
    {
        auto result (amount.number () / germ::kxrb_ratio);
        boost::property_tree::ptree response_l;
        response_l.put ("amount", result.convert_to<std::string> ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad amount number");
    }
}

void germ::rpc_handler::krai_to_raw ()
{
    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (amount.decode_dec (amount_text))
    {
        error_response (response, "Bad amount number");
        return;
    }

    auto result (amount.number () * germ::kxrb_ratio);
    if (result <= amount.number ())
    {
        error_response (response, "Amount too big");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("amount", result.convert_to<std::string> ());
    response (response_l);
}

void germ::rpc_handler::password_change ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, true);
    boost::property_tree::ptree response_l;
    std::string password_text (request.get<std::string> ("password"));
    auto error_change (existing->second->store.rekey (transaction, password_text));
    response_l.put ("changed", error_change ? "0" : "1");
    response (response_l);
}

void germ::rpc_handler::password_enter ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    std::string password_text (request.get<std::string> ("password"));
    auto error_valid (existing->second->enter_password (password_text));
    response_l.put ("valid", error_valid ? "0" : "1");
    response (response_l);
}

void germ::rpc_handler::password_valid (bool wallet_locked = false)
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    boost::property_tree::ptree response_l;
    auto valid (existing->second->store.valid_password (transaction));
    if (!wallet_locked)
    {
        response_l.put ("valid", valid ? "1" : "0");
    }
    else
    {
        response_l.put ("locked", valid ? "0" : "1");
    }
    response (response_l);
}

void germ::rpc_handler::peers ()
{
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree peers_l;
    auto peers_list (node.peers.list_version ());
    for (auto i (peers_list.begin ()), n (peers_list.end ()); i != n; ++i)
    {
        std::stringstream text;
        text << i->first;
        peers_l.push_back (boost::property_tree::ptree::value_type (text.str (), boost::property_tree::ptree (std::to_string (i->second))));
    }
    response_l.add_child ("peers", peers_l);
    response (response_l);
}

void germ::rpc_handler::pending ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    if (account.decode_account (account_text))
    {
        error_response (response, "Bad account number");
        return;
    }

    uint64_t count (std::numeric_limits<uint64_t>::max ());
    germ::uint128_union threshold (0);
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    boost::optional<std::string> threshold_text (request.get_optional<std::string> ("threshold"));
    if (threshold_text.is_initialized ())
    {
        auto error_threshold (threshold.decode_dec (threshold_text.get ()));
        if (error_threshold)
        {
            error_response (response, "Bad threshold number");
        }
    }
    const bool source = request.get<bool> ("source", false);
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree peers_l;
    {
        germ::transaction transaction (node.store.environment, nullptr, false);
        germ::account end (account.number () + 1);
        for (auto i (node.store.pending_begin (transaction, germ::pending_key (account, 0))), n (node.store.pending_begin (transaction, germ::pending_key (end, 0))); i != n && peers_l.size () < count; ++i)
        {
            germ::pending_key key (i->first);
            if (threshold.is_zero () && !source)
            {
                boost::property_tree::ptree entry;
                entry.put ("", key.hash.to_string ());
                peers_l.push_back (std::make_pair ("", entry));
            }
            else
            {
                germ::pending_info info (i->second);
                if (info.amount.number () >= threshold.number ())
                {
                    if (source)
                    {
                        boost::property_tree::ptree pending_tree;
                        pending_tree.put ("amount", info.amount.number ().convert_to<std::string> ());
                        pending_tree.put ("source", info.source.to_account ());
                        peers_l.add_child (key.hash.to_string (), pending_tree);
                    }
                    else
                    {
                        peers_l.put (key.hash.to_string (), info.amount.number ().convert_to<std::string> ());
                    }
                }
            }
        }
    }
    response_l.add_child ("blocks", peers_l);
    response (response_l);
}

void germ::rpc_handler::pending_exists ()
{
    std::string hash_text (request.get<std::string> ("hash"));
    germ::uint256_union hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad hash number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto block (node.store.block_get (transaction, hash));
    if (block == nullptr)
    {
        error_response (response, "Block not found");
        return;
    }

    auto exists (false);
    auto destination (node.ledger.block_destination (transaction, *block));
    if (!destination.is_zero ())
    {
        exists = node.store.pending_exists (transaction, germ::pending_key (destination, hash));
    }
    boost::property_tree::ptree response_l;
    response_l.put ("exists", exists ? "1" : "0");
    response (response_l);
}

void germ::rpc_handler::payment_begin ()
{
    std::string id_text (request.get<std::string> ("wallet"));
    germ::uint256_union id;
    if (id.decode_hex (id_text))
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (id));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Unable to find wallets");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, true);
    std::shared_ptr<germ::wallet> wallet (existing->second);
    if (!wallet->store.valid_password (transaction))
    {
        error_response (response, "Wallet locked");
        return;
    }

    germ::account account (0);
    do
    {
        auto existing (wallet->free_accounts.begin ());
        if (existing != wallet->free_accounts.end ())
        {
            account = *existing;
            wallet->free_accounts.erase (existing);
            if (wallet->store.find (transaction, account) == wallet->store.end ())
            {
                BOOST_LOG (node.log) << boost::str (boost::format ("Transaction wallet %1% externally modified listing account %2% as free but no longer exists") % id.to_string () % account.to_account ());
                account.clear ();
            }
            else
            {
                if (!node.ledger.account_balance (transaction, account).is_zero ())
                {
                    BOOST_LOG (node.log) << boost::str (boost::format ("Skipping account %1% for use as a transaction account: non-zero balance") % account.to_account ());
                    account.clear ();
                }
            }
        }
        else
        {
            account = wallet->deterministic_insert (transaction);
            break;
        }
    } while (account.is_zero ());

    if (!account.is_zero ())
    {
        boost::property_tree::ptree response_l;
        response_l.put ("account", account.to_account ());
        response (response_l);
    }
    else
    {
        error_response (response, "Unable to create transaction account");
    }
}

void germ::rpc_handler::payment_init ()
{
    std::string id_text (request.get<std::string> ("wallet"));
    germ::uint256_union id;
    if (id.decode_hex (id_text))
    {
        error_response (response, "Bad transaction wallet number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, true);
    auto existing (node.wallets.items.find (id));
    if (existing == node.wallets.items.end ())
    {
        boost::property_tree::ptree response_l;
        response_l.put ("status", "Unable to find transaction wallet");
        response (response_l);
        return;
    }

    auto wallet (existing->second);
    if (wallet->store.valid_password (transaction))
    {
        wallet->init_free_accounts (transaction);
        boost::property_tree::ptree response_l;
        response_l.put ("status", "Ready");
        response (response_l);
    }
    else
    {
        boost::property_tree::ptree response_l;
        response_l.put ("status", "Transaction wallet locked");
        response (response_l);
    }
}

void germ::rpc_handler::payment_end ()
{
    std::string id_text (request.get<std::string> ("wallet"));
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union id;
    if (id.decode_hex (id_text))
    {
        error_response (response, "Bad wallet number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto existing (node.wallets.items.find (id));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Unable to find wallet");
        return;
    }

    auto wallet (existing->second);
    germ::account account;
    if (account.decode_account (account_text))
    {
        error_response (response, "Invalid account number");
        return;
    }

    auto existing_acc (wallet->store.find (transaction, account));
    if (existing_acc == wallet->store.end ())
    {
        error_response (response, "Account not in wallet");
        return;
    }

    if (!node.ledger.account_balance (transaction, account).is_zero ())
    {
        error_response (response, "Account has non-zero balance");
        return;
    }

    wallet->free_accounts.insert (account);
    boost::property_tree::ptree response_l;
    response (response_l);
}

void germ::rpc_handler::payment_wait ()
{
    std::string account_text (request.get<std::string> ("account"));
    std::string amount_text (request.get<std::string> ("amount"));
    std::string timeout_text (request.get<std::string> ("timeout"));
    germ::uint256_union account;
    if (account.decode_account (account_text))
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::uint128_union amount;
    if (amount.decode_dec (amount_text))
    {
        error_response (response, "Bad amount number");
        return;
    }

    uint64_t timeout;
    if (decode_unsigned (timeout_text, timeout))
    {
        error_response (response, "Bad timeout number");
        return;
    }

    {
        auto observer (std::make_shared<germ::payment_observer> (response, rpc, account, amount));
        observer->start (timeout);
        std::lock_guard<std::mutex> lock (rpc.mutex);
        assert (rpc.payment_observers.find (account) == rpc.payment_observers.end ());
        rpc.payment_observers[account] = observer;
    }
    rpc.observer_action (account);
}

void germ::rpc_handler::process ()
{
    std::string block_text (request.get<std::string> ("block"));
    boost::property_tree::ptree block_l;
    std::stringstream block_stream (block_text);
    boost::property_tree::read_json (block_stream, block_l);
    std::shared_ptr<germ::tx> block (germ::deserialize_block_json (block_l));
    if (block == nullptr)
    {
        error_response (response, "Block is invalid");
        return;
    }

//    if (!germ::work_validate (*block))
//    {
//        error_response (response, "Block work is invalid");
//        return;
//    }

    auto hash (block->hash ());
    node.block_arrival.add (hash);
    germ::process_return result;
    {
        germ::transaction transaction (node.store.environment, nullptr, true);
        result = node.block_processor.process_receive_one (transaction, block, std::chrono::steady_clock::time_point ());
    }
    switch (result.code)
    {
        case germ::process_result::progress:
        {
            boost::property_tree::ptree response_l;
            response_l.put ("hash", hash.to_string ());
            response (response_l);
            break;
        }
        case germ::process_result::gap_previous:
        {
            error_response (response, "Gap previous block");
            break;
        }
        case germ::process_result::gap_source:
        {
            error_response (response, "Gap source block");
            break;
        }
        case germ::process_result::old:
        {
            error_response (response, "Old block");
            break;
        }
        case germ::process_result::bad_signature:
        {
            error_response (response, "Bad signature");
            break;
        }
        case germ::process_result::negative_spend:
        {
            // TODO once we get RPC versioning, this should be changed to "negative spend"
            error_response (response, "Overspend");
            break;
        }
        case germ::process_result::unreceivable:
        {
            error_response (response, "Unreceivable");
            break;
        }
        case germ::process_result::fork:
        {
            const bool force = request.get<bool> ("force", false);
            if (force && rpc.config.enable_control)
            {
                node.active.erase (*block);
                node.block_processor.force (block);
                boost::property_tree::ptree response_l;
                response_l.put ("hash", hash.to_string ());
                response (response_l);
            }
            else
            {
                error_response (response, "Fork");
            }
            break;
        }
        default:
        {
            error_response (response, "Error processing block");
            break;
        }
    }
}

void germ::rpc_handler::rai_from_raw ()
{
    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (!amount.decode_dec (amount_text))
    {
        auto result (amount.number () / germ::xrb_ratio);
        boost::property_tree::ptree response_l;
        response_l.put ("amount", result.convert_to<std::string> ());
        response (response_l);
    }
    else
    {
        error_response (response, "Bad amount number");
    }
}

void germ::rpc_handler::rai_to_raw ()
{
    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (amount.decode_dec (amount_text))
    {
        error_response (response, "Bad amount number");
        return;
    }

    auto result (amount.number () * germ::xrb_ratio);
    if (result <= amount.number ())
    {
        error_response (response, "Amount too big");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("amount", result.convert_to<std::string> ());
    response (response_l);
}

void germ::rpc_handler::receive ()
{
    /**
     *  output hash: 4F5A4BFDEF20286D0B331AA9CDD00F6636A53884F1CFBE69CE42A13D8182A826
        45B7CD0F666138749EBF67699EB7FCDB7BE9A0B4ABFEA8F3509C43F5086EDA5D
        xrb_3r6jh8mh5ercukwhsaxbj9opiqjw4udx4rtqu8y8ki9cmyze4d7c7r3gdmya
        B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0
        340282366920938463463374607421686211445
        200
        eeeeeeeee
        40
        10
        0000000000000000000000000000000000000000000000000000000000000000

        input hash: A3B0720655C312B40F4CABA3BE62927C62A20D4A7A26A9CA766A35A8782F68B4
        45B7CD0F666138749EBF67699EB7FCDB7BE9A0B4ABFEA8F3509C43F5086EDA5D
        xrb_3r6jh8mh5ercukwhsaxbj9opiqjw4udx4rtqu8y8ki9cmyze4d7c7r3gdmya
        B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0
        340282366920938463463374607421686211445
        200
        eeeeeeeee
        40
        10
        0000000000000000000000000000000000000000000000000000000000000000
     */

    germ::block_hash previ("45B7CD0F666138749EBF67699EB7FCDB7BE9A0B4ABFEA8F3509C43F5086EDA5D");
    germ::account desti("xrb_3r6jh8mh5ercukwhsaxbj9opiqjw4udx4rtqu8y8ki9cmyze4d7c7r3gdmya");
    germ::block_hash source("B0311EA55708D6A53C75CDBF88300259C6D018522FE3D4D0A242E431F9E8B6D0");
    germ::amount balanc("340282366920938463463374607421686211445");
    germ::tx_message txs(200, "eeeeeeeee", 40, 10);
    germ::epoch_hash epoch("0000000000000000000000000000000000000000000000000000000000000000");
    germ::raw_key prv;
    germ::public_key pub;
    germ::tx tx(previ, desti, source, desti, balanc, txs, epoch, prv, pub);

    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error_acc (account.decode_account (account_text));
    if (error_acc)
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto account_check (existing->second->store.find (transaction, account));
    if (account_check == existing->second->store.end ())
    {
        error_response (response, "Account not found in wallet");
        return;
    }

    std::string hash_text (request.get<std::string> ("block"));
    germ::uint256_union hash;
    auto error_blk (hash.decode_hex (hash_text));
    if (error_blk)
    {
        error_response (response, "Bad block number");
        return;
    }

    auto block (node.store.block_get (transaction, hash));
    if (block == nullptr)
    {
        error_response (response, "Block not found");
        return;
    }

    if (!node.store.pending_exists (transaction, germ::pending_key (account, hash)))
    {
        error_response (response, "Block is not available to receive");
        return;
    }

    uint64_t work (0);
    boost::optional<std::string> work_text (request.get_optional<std::string> ("work"));
    auto error_work (false);
    if (work_text.is_initialized ())
    {
        error_work = germ::from_string_hex (work_text.get (), work);
        if (error_work)
        {
            error_response (response, "Bad work");
        }
    }
    if (work)
    {
        germ::account_info info;
        germ::uint256_union head;
        if (node.store.account_get (transaction, account, info))
        {
            head = info.head;
        }
        else
        {
            head = account;
        }
        if (germ::work_validate (head, work))
        {
            germ::transaction transaction_a (node.store.environment, nullptr, true);
            existing->second->store.work_put (transaction_a, account, work);
        }
        else
        {
            error_work = true;
            error_response (response, "Invalid work");
        }
    }
    if (!error_work)
    {
        auto response_a (response);
        existing->second->receive_async (std::move (block), account, germ::genesis_amount, [response_a](std::shared_ptr<germ::tx> block_a) {
                                             germ::uint256_union hash_a (0);
                                             if (block_a != nullptr)
                                             {
                                                 hash_a = block_a->hash ();
                                             }
                                             boost::property_tree::ptree response_l;
                                             response_l.put ("block", hash_a.to_string ());
                                             response_a (response_l);
                                         },
                                         work == 0);
    }
}

void germ::rpc_handler::receive_minimum ()
{
    if (rpc.config.enable_control)
    {
        boost::property_tree::ptree response_l;
        response_l.put ("amount", node.config.receive_minimum.to_string_dec ());
        response (response_l);
    }
    else
    {
        error_response (response, "RPC control is disabled");
    }
}

void germ::rpc_handler::receive_minimum_set ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string amount_text (request.get<std::string> ("amount"));
    germ::uint128_union amount;
    if (amount.decode_dec (amount_text))
    {
        error_response (response, "Bad amount number");
        return;
    }

    node.config.receive_minimum = amount;
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::representatives ()
{
    uint64_t count (std::numeric_limits<uint64_t>::max ());
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    const bool sorting = request.get<bool> ("sorting", false);
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree representatives;
    germ::transaction transaction (node.store.environment, nullptr, false);
//    if (!sorting) // Simple
//    {
//        for (auto i (node.store.representation_begin (transaction)), n (node.store.representation_end ()); i != n && representatives.size () < count; ++i)
//        {
//            germ::account account (i->first.uint256 ());
//            auto amount (node.store.representation_get (transaction, account));
//            representatives.put (account.to_account (), amount.convert_to<std::string> ());
//        }
//    }
//    else // Sorting
//    {
//        std::vector<std::pair<germ::uint128_union, std::string>> representation;
//        for (auto i (node.store.representation_begin (transaction)), n (node.store.representation_end ()); i != n; ++i)
//        {
//            germ::account account (i->first.uint256 ());
//            auto amount (node.store.representation_get (transaction, account));
//            representation.push_back (std::make_pair (amount, account.to_account ()));
//        }
//        std::sort (representation.begin (), representation.end ());
//        std::reverse (representation.begin (), representation.end ());
//        for (auto i (representation.begin ()), n (representation.end ()); i != n && representatives.size () < count; ++i)
//        {
//            representatives.put (i->second, (i->first).number ().convert_to<std::string> ());
//        }
//    }
    response_l.add_child ("representatives", representatives);
    response (response_l);
}

void germ::rpc_handler::representatives_online ()
{
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree representatives;
//    auto reps (node.online_reps.list ());
//    for (auto & i : reps)
//    {
//        representatives.put (i.to_account (), "");
//    }
    response_l.add_child ("representatives", representatives);
    response (response_l);
}

void germ::rpc_handler::republish ()
{
    uint64_t count (1024U);
    uint64_t sources (0);
    uint64_t destinations (0);
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    boost::optional<std::string> sources_text (request.get_optional<std::string> ("sources"));
    if (sources_text.is_initialized ())
    {
        auto sources_error (decode_unsigned (sources_text.get (), sources));
        if (sources_error)
        {
            error_response (response, "Invalid sources number");
        }
    }
    boost::optional<std::string> destinations_text (request.get_optional<std::string> ("destinations"));
    if (destinations_text.is_initialized ())
    {
        auto destinations_error (decode_unsigned (destinations_text.get (), destinations));
        if (destinations_error)
        {
            error_response (response, "Invalid destinations number");
        }
    }
    std::string hash_text (request.get<std::string> ("hash"));
    germ::uint256_union hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad hash number");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree blocks;
    germ::transaction transaction (node.store.environment, nullptr, false);
    auto block (node.store.block_get (transaction, hash));
    if (block == nullptr)
    {
        error_response (response, "Block not found");
        return;
    }

    for (auto i (0); !hash.is_zero () && i < count; ++i)
    {
        block = node.store.block_get (transaction, hash);
        if (sources != 0) // Republish source chain
        {
            germ::block_hash source (node.ledger.block_source (transaction, *block));
            std::unique_ptr<germ::tx> block_a (node.store.block_get (transaction, source));
            std::vector<germ::block_hash> hashes;
            while (block_a != nullptr && hashes.size () < sources)
            {
                hashes.push_back (source);
                source = block_a->previous ();
                block_a = node.store.block_get (transaction, source);
            }
            std::reverse (hashes.begin (), hashes.end ());
            for (auto & hash_l : hashes)
            {
                block_a = node.store.block_get (transaction, hash_l);
                node.network.republish_block (transaction, std::move (block_a));
                boost::property_tree::ptree entry_l;
                entry_l.put ("", hash_l.to_string ());
                blocks.push_back (std::make_pair ("", entry_l));
            }
        }
        node.network.republish_block (transaction, std::move (block)); // Republish block
        boost::property_tree::ptree entry;
        entry.put ("", hash.to_string ());
        blocks.push_back (std::make_pair ("", entry));
        if (destinations != 0) // Republish destination chain
        {
            auto block_b (node.store.block_get (transaction, hash));
            auto destination (node.ledger.block_destination (transaction, *block_b));
            if (!destination.is_zero ())
            {
                auto exists (node.store.pending_exists (transaction, germ::pending_key (destination, hash)));
                if (!exists)
                {
                    germ::block_hash previous (node.ledger.latest (transaction, destination));
                    std::unique_ptr<germ::tx> block_d (node.store.block_get (transaction, previous));
                    germ::block_hash source;
                    std::vector<germ::block_hash> hashes;
                    while (block_d != nullptr && hash != source)
                    {
                        hashes.push_back (previous);
                        source = node.ledger.block_source (transaction, *block_d);
                        previous = block_d->previous ();
                        block_d = node.store.block_get (transaction, previous);
                    }
                    std::reverse (hashes.begin (), hashes.end ());
                    if (hashes.size () > destinations)
                    {
                        hashes.resize (destinations);
                    }
                    for (auto & hash_l : hashes)
                    {
                        block_d = node.store.block_get (transaction, hash_l);
                        node.network.republish_block (transaction, std::move (block_d));
                        boost::property_tree::ptree entry_l;
                        entry_l.put ("", hash_l.to_string ());
                        blocks.push_back (std::make_pair ("", entry_l));
                    }
                }
            }
        }
        hash = node.store.block_successor (transaction, hash);
    }
    response_l.put ("success", ""); // obsolete
    response_l.add_child ("blocks", blocks);
    response (response_l);
}

void germ::rpc_handler::search_pending ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
        return;

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    auto error_start (existing->second->search_pending ());
    boost::property_tree::ptree response_l;
    response_l.put ("started", !error);
    response (response_l);
}

void germ::rpc_handler::search_pending_all ()
{
    if (!rpc.config.enable_control)
    {
        error_response(response, "RPC control is disabled");
        return;
    }

    node.wallets.search_pending_all ();
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::send ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    std::string source_text (request.get<std::string> ("source"));
    germ::account source;
    auto error_src (source.decode_account (source_text));
    if (error_src)
    {
        error_response (response, "Bad source account");
        return;
    }

    std::string destination_text (request.get<std::string> ("destination"));
    germ::account destination;
    auto error_dst (destination.decode_account (destination_text));
    if (error_dst)
    {
        error_response (response, "Bad destination account");
        return;
    }

    std::string amount_text (request.get<std::string> ("amount"));
    germ::amount amount;
    auto error_amount (amount.decode_dec (amount_text));
    if (error_amount)
    {
        error_response (response, "Bad amount format");
        return;
    }

    uint64_t work (0);
    boost::optional<std::string> work_text (request.get_optional<std::string> ("work"));
    if (work_text.is_initialized ())
    {
        error_amount = germ::from_string_hex (work_text.get (), work);
        if (error_amount)
        {
            error_response (response, "Bad work");
        }
    }
    germ::uint128_t balance (0);
    if (!error_amount)
    {
        germ::transaction transaction (node.store.environment, nullptr, work != 0); // false if no "work" in request, true if work > 0
        germ::account_info info;
        if (node.store.account_get (transaction, source, info))
        {
            balance = (info.balance).number ();
        }
        else
        {
            error_amount = true;
            error_response (response, "Account not found");
        }
        if (!error_amount && work)
        {
            if (germ::work_validate (info.head, work))
            {
                existing->second->store.work_put (transaction, source, work);
            }
            else
            {
                error_amount = true;
                error_response (response, "Invalid work");
            }
        }
    }

    if (error_amount)
        return;

    boost::optional<std::string> send_id (request.get_optional<std::string> ("id"));
    if (balance < amount.number ())
    {
        error_response (response, "Insufficient balance");
        return;
    }

    auto rpc_l (shared_from_this ());
    auto response_a (response);
    existing->second->send_async (source, destination, amount.number (), [response_a](std::shared_ptr<germ::tx> block_a) {
                                      if (block_a != nullptr)
                                      {
                                          germ::uint256_union hash (block_a->hash ());
                                          boost::property_tree::ptree response_l;
                                          response_l.put ("block", hash.to_string ());
                                          response_a (response_l);
                                      }
                                      else
                                      {
                                          error_response (response_a, "Error generating block");
                                      }
                                  },
                                  work == 0, send_id);
}

void germ::rpc_handler::stats ()
{
    bool error = false;
    auto sink = node.stats.log_sink_json ();
    std::string type (request.get<std::string> ("type", ""));
    if (type == "counters")
    {
        node.stats.log_counters (*sink);
    }
    else if (type == "samples")
    {
        node.stats.log_samples (*sink);
    }
    else
    {
        error = true;
        error_response (response, "Invalid or missing type argument");
    }

    if (!error)
    {
        response (*static_cast<boost::property_tree::ptree *> (sink->to_object ()));
    }
}

void germ::rpc_handler::stop ()
{
    if (rpc.config.enable_control)
    {
        boost::property_tree::ptree response_l;
        response_l.put ("success", "");
        response (response_l);
        rpc.stop ();
        node.stop ();
    }
    else
    {
        error_response (response, "RPC control is disabled");
    }
}

void germ::rpc_handler::unchecked ()
{
    uint64_t count (std::numeric_limits<uint64_t>::max ());
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree unchecked;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (node.store.unchecked_begin (transaction)), n (node.store.unchecked_end ()); i != n && unchecked.size () < count; ++i)
    {
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
        auto block (germ::deserialize_block (stream));
        std::string contents;
        block->serialize_json (contents);
        unchecked.put (block->hash ().to_string (), contents);
    }
    response_l.add_child ("blocks", unchecked);
    response (response_l);
}

void germ::rpc_handler::unchecked_clear ()
{
    if (rpc.config.enable_control)
    {
        germ::transaction transaction (node.store.environment, nullptr, true);
        node.store.unchecked_clear (transaction);
        boost::property_tree::ptree response_l;
        response_l.put ("success", "");
        response (response_l);
    }
    else
    {
        error_response (response, "RPC control is disabled");
    }
}

void germ::rpc_handler::unchecked_get ()
{
    std::string hash_text (request.get<std::string> ("hash"));
    germ::uint256_union hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad hash number");
        return;
    }

    boost::property_tree::ptree response_l;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (node.store.unchecked_begin (transaction)), n (node.store.unchecked_end ()); i != n; ++i)
    {
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
        auto block (germ::deserialize_block (stream));
        if (block->hash () == hash)
        {
            std::string contents;
            block->serialize_json (contents);
            response_l.put ("contents", contents);
            break;
        }
    }
    if (response_l.empty ())
    {
        error_response (response, "Unchecked block not found");
        return;
    }

    response (response_l);
}

void germ::rpc_handler::unchecked_keys ()
{
    uint64_t count (std::numeric_limits<uint64_t>::max ());
    germ::uint256_union key (0);
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    boost::optional<std::string> hash_text (request.get_optional<std::string> ("key"));
    if (hash_text.is_initialized ())
    {
        auto error_hash (key.decode_hex (hash_text.get ()));
        if (error_hash)
        {
            error_response (response, "Bad key hash number");
        }
    }
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree unchecked;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (node.store.unchecked_begin (transaction, key)), n (node.store.unchecked_end ()); i != n && unchecked.size () < count; ++i)
    {
        boost::property_tree::ptree entry;
        germ::bufferstream stream (reinterpret_cast<uint8_t const *> (i->second.data ()), i->second.size ());
        auto block (germ::deserialize_block (stream));
        std::string contents;
        block->serialize_json (contents);
        entry.put ("key", germ::block_hash (i->first.uint256 ()).to_string ());
        entry.put ("hash", block->hash ().to_string ());
        entry.put ("contents", contents);
        unchecked.push_back (std::make_pair ("", entry));
    }
    response_l.add_child ("unchecked", unchecked);
    response (response_l);
}

void germ::rpc_handler::version ()
{
    boost::property_tree::ptree response_l;
    response_l.put ("rpc_version", "1");
    response_l.put ("store_version", std::to_string (node.store_version ()));
    response_l.put ("node_vendor", boost::str (boost::format ("RaiBlocks %1%.%2%") % GERMBLOCKS_VERSION_MAJOR % GERMBLOCKS_VERSION_MINOR));
    response (response_l);
}

void germ::rpc_handler::validate_account_number ()
{
    std::string account_text (request.get<std::string> ("account"));
    germ::uint256_union account;
    auto error (account.decode_account (account_text));
    boost::property_tree::ptree response_l;
    response_l.put ("valid", error ? "0" : "1");
    response (response_l);
}

void germ::rpc_handler::wallet_add ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string key_text (request.get<std::string> ("key"));
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::raw_key key;
    auto error (key.data.decode_hex (key_text));
    if (error)
    {
        error_response (response, "Bad private key");
        return;
    }

    germ::uint256_union wallet;
    auto error_wallet (wallet.decode_hex (wallet_text));
    if (error_wallet)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    const bool generate_work = request.get<bool> ("work", true);
    auto pub (existing->second->insert_adhoc (key, generate_work));
    if (pub.is_zero ())
    {
        error_response (response, "Wallet locked");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("account", pub.to_account ());
    response (response_l);
}

void germ::rpc_handler::wallet_add_watch ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, true);
    if (!existing->second->store.valid_password (transaction))
    {
        error_response (response, "Wallet locked");
        return;
    }

    for (auto & accounts : request.get_child ("accounts"))
    {
        std::string account_text = accounts.second.data ();
        germ::uint256_union account;
        auto error (account.decode_account (account_text));
        if (!error)
        {
            existing->second->insert_watch (transaction, account);
        }
        else
        {
            error_response (response, "Bad account number");
        }
    }
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::wallet_balance_total ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::uint128_t balance (0);
    germ::uint128_t pending (0);
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        balance = balance + node.ledger.account_balance (transaction, account);
        pending = pending + node.ledger.account_pending (transaction, account);
    }
    boost::property_tree::ptree response_l;
    response_l.put ("balance", balance.convert_to<std::string> ());
    response_l.put ("pending", pending.convert_to<std::string> ());
    response (response_l);
}

void germ::rpc_handler::wallet_balances ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    germ::uint128_union threshold (0);
    boost::optional<std::string> threshold_text (request.get_optional<std::string> ("threshold"));
    if (threshold_text.is_initialized ())
    {
        auto error_threshold (threshold.decode_dec (threshold_text.get ()));
        if (error_threshold)
        {
            error_response (response, "Bad threshold number");
        }
    }
    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree balances;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        germ::uint128_t balance = node.ledger.account_balance (transaction, account);
        if (threshold.is_zero ())
        {
            boost::property_tree::ptree entry;
            germ::uint128_t pending = node.ledger.account_pending (transaction, account);
            entry.put ("balance", balance.convert_to<std::string> ());
            entry.put ("pending", pending.convert_to<std::string> ());
            balances.push_back (std::make_pair (account.to_account (), entry));
        }
        else
        {
            if (balance >= threshold.number ())
            {
                boost::property_tree::ptree entry;
                germ::uint128_t pending = node.ledger.account_pending (transaction, account);
                entry.put ("balance", balance.convert_to<std::string> ());
                entry.put ("pending", pending.convert_to<std::string> ());
                balances.push_back (std::make_pair (account.to_account (), entry));
            }
        }
    }
    response_l.add_child ("balances", balances);
    response (response_l);
}

void germ::rpc_handler::wallet_change_seed ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string seed_text (request.get<std::string> ("seed"));
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::raw_key seed;
    auto error (seed.data.decode_hex (seed_text));
    if (error)
    {
        error_response (response, "Bad seed");
        return;
    }

    germ::uint256_union wallet;
    auto error_wallet (wallet.decode_hex (wallet_text));
    if (error_wallet)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, true);
    if (!existing->second->store.valid_password (transaction))
    {
        error_response (response, "Wallet locked");
        return;
    }

    existing->second->change_seed (transaction, seed);
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::wallet_contains ()
{
    std::string account_text (request.get<std::string> ("account"));
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union account;
    auto error (account.decode_account (account_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::uint256_union wallet;
    auto error_wallet (wallet.decode_hex (wallet_text));
    if (error_wallet)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto exists (existing->second->store.find (transaction, account) != existing->second->store.end ());
    boost::property_tree::ptree response_l;
    response_l.put ("exists", exists ? "1" : "0");
    response (response_l);
}

void germ::rpc_handler::wallet_create ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    germ::keypair wallet_id;
    node.wallets.create (wallet_id.pub);
    germ::transaction transaction (node.store.environment, nullptr, false);
    auto existing (node.wallets.items.find (wallet_id.pub));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Failed to create wallet. Increase lmdb_max_dbs in node config.");
        return;
    }

    boost::property_tree::ptree response_l;
    response_l.put ("wallet", wallet_id.pub.to_string ());
    response (response_l);
}

void germ::rpc_handler::wallet_destroy ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    node.wallets.destroy (wallet);
    boost::property_tree::ptree response_l;
    response (response_l);
}

void germ::rpc_handler::wallet_export ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    std::string json;
    existing->second->store.serialize_json (transaction, json);
    boost::property_tree::ptree response_l;
    response_l.put ("json", json);
    response (response_l);
}

void germ::rpc_handler::wallet_frontiers ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree frontiers;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        auto latest (node.ledger.latest (transaction, account));
        if (!latest.is_zero ())
        {
            frontiers.put (account.to_account (), latest.to_string ());
        }
    }
    response_l.add_child ("frontiers", frontiers);
    response (response_l);
}

void germ::rpc_handler::wallet_key_valid ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto valid (existing->second->store.valid_password (transaction));
    boost::property_tree::ptree response_l;
    response_l.put ("valid", valid ? "1" : "0");
    response (response_l);
}

void germ::rpc_handler::wallet_ledger ()
{
//    const bool representative = request.get<bool> ("representative", false);
    const bool weight = request.get<bool> ("weight", false);
    const bool pending = request.get<bool> ("pending", false);
    uint64_t modified_since (0);
    boost::optional<std::string> modified_since_text (request.get_optional<std::string> ("modified_since"));
    if (modified_since_text.is_initialized ())
    {
        modified_since = strtoul (modified_since_text.get ().c_str (), NULL, 10);
    }
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree accounts;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        germ::account_info info;
        if (node.store.account_get (transaction, account, info))
        {
            if (info.modified >= modified_since)
            {
                boost::property_tree::ptree entry;
                entry.put ("frontier", info.head.to_string ());
                entry.put ("open_block", info.open_block.to_string ());
//                entry.put ("representative_block", info.rep_block.to_string ());
                std::string balance;
                germ::uint128_union (info.balance).encode_dec (balance);
                entry.put ("balance", balance);
                entry.put ("modified_timestamp", std::to_string (info.modified));
                entry.put ("block_count", std::to_string (info.block_count));
//                if (representative)
//                {
//                    auto block (node.store.block_get (transaction, info.rep_block));
//                    assert (block != nullptr);
//                    entry.put ("representative", block->representative ().to_account ());
//                }
                if (weight)
                {
                    auto account_weight (node.ledger.weight (transaction, account));
                    entry.put ("weight", account_weight.convert_to<std::string> ());
                }
                if (pending)
                {
                    auto account_pending (node.ledger.account_pending (transaction, account));
                    entry.put ("pending", account_pending.convert_to<std::string> ());
                }
                accounts.push_back (std::make_pair (account.to_account (), entry));
            }
        }
    }
    response_l.add_child ("accounts", accounts);
    response (response_l);
}

void germ::rpc_handler::wallet_lock ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    germ::raw_key empty;
    empty.data.clear ();
    existing->second->store.password.value_set (empty);
    response_l.put ("locked", "1");
    response (response_l);
}

void germ::rpc_handler::wallet_pending ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response(response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    uint64_t count (std::numeric_limits<uint64_t>::max ());
    germ::uint128_union threshold (0);
    boost::optional<std::string> count_text (request.get_optional<std::string> ("count"));
    if (count_text.is_initialized ())
    {
        auto error (decode_unsigned (count_text.get (), count));
        if (error)
        {
            error_response (response, "Invalid count limit");
        }
    }
    boost::optional<std::string> threshold_text (request.get_optional<std::string> ("threshold"));
    if (threshold_text.is_initialized ())
    {
        auto error_threshold (threshold.decode_dec (threshold_text.get ()));
        if (error_threshold)
        {
            error_response (response, "Bad threshold number");
        }
    }
    const bool source = request.get<bool> ("source", false);
    boost::property_tree::ptree response_l;
    boost::property_tree::ptree pending;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        boost::property_tree::ptree peers_l;
        germ::account end (account.number () + 1);
        for (auto ii (node.store.pending_begin (transaction, germ::pending_key (account, 0))), nn (node.store.pending_begin (transaction, germ::pending_key (end, 0))); ii != nn && peers_l.size () < count; ++ii)
        {
            germ::pending_key key (ii->first);
            if (threshold.is_zero () && !source)
            {
                boost::property_tree::ptree entry;
                entry.put ("", key.hash.to_string ());
                peers_l.push_back (std::make_pair ("", entry));
            }
            else
            {
                germ::pending_info info (ii->second);
                if (info.amount.number () >= threshold.number ())
                {
                    if (source)
                    {
                        boost::property_tree::ptree pending_tree;
                        pending_tree.put ("amount", info.amount.number ().convert_to<std::string> ());
                        pending_tree.put ("source", info.source.to_account ());
                        peers_l.add_child (key.hash.to_string (), pending_tree);
                    }
                    else
                    {
                        peers_l.put (key.hash.to_string (), info.amount.number ().convert_to<std::string> ());
                    }
                }
            }
        }
        if (!peers_l.empty ())
        {
            pending.add_child (account.to_account (), peers_l);
        }
    }
    response_l.add_child ("blocks", pending);
    response (response_l);
}

void germ::rpc_handler::wallet_representative ()
{
    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    boost::property_tree::ptree response_l;
//    response_l.put ("representative", existing->second->store.representative (transaction).to_account ());
    response_l.put ("representative", "");
    response (response_l);
}

void germ::rpc_handler::wallet_representative_set ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad account number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

//    std::string representative_text (request.get<std::string> ("representative"));
//    germ::account representative;
//    auto error_acc (representative.decode_account (representative_text));
//    if (error_acc)
//    {
//        error_response (response, "Invalid account number");
//        return;
//    }

    germ::transaction transaction (node.store.environment, nullptr, true);
//    existing->second->store.representative_set (transaction, representative);
    boost::property_tree::ptree response_l;
    response_l.put ("set", "1");
    response (response_l);
}

void germ::rpc_handler::wallet_republish ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    uint64_t count;
    std::string count_text (request.get<std::string> ("count"));
    auto error_count (decode_unsigned (count_text, count));
    if (error_count)
    {
        error_response (response, "Invalid count limit");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree blocks;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        auto latest (node.ledger.latest (transaction, account));
        std::unique_ptr<germ::tx> block;
        std::vector<germ::block_hash> hashes;
        while (!latest.is_zero () && hashes.size () < count)
        {
            hashes.push_back (latest);
            block = node.store.block_get (transaction, latest);
            latest = block->previous ();
        }
        std::reverse (hashes.begin (), hashes.end ());
        for (auto & hash : hashes)
        {
            block = node.store.block_get (transaction, hash);
            node.network.republish_block (transaction, std::move (block));
            ;
            boost::property_tree::ptree entry;
            entry.put ("", hash.to_string ());
            blocks.push_back (std::make_pair ("", entry));
        }
    }
    response_l.add_child ("blocks", blocks);
    response (response_l);
}

void germ::rpc_handler::wallet_work_get ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    boost::property_tree::ptree response_l;
    boost::property_tree::ptree works;
    germ::transaction transaction (node.store.environment, nullptr, false);
    for (auto i (existing->second->store.begin (transaction)), n (existing->second->store.end ()); i != n; ++i)
    {
        germ::account account (i->first.uint256 ());
        uint64_t work (0);
        auto error_work (existing->second->store.work_get (transaction, account, work));
        works.put (account.to_account (), germ::to_string_hex (work));
    }
    response_l.add_child ("works", works);
    response (response_l);
}

void germ::rpc_handler::work_generate ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string hash_text (request.get<std::string> ("hash"));
    bool use_peers (request.get_optional<bool> ("use_peers") == true);
    germ::block_hash hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad block hash");
        return;
    }

    auto rpc_l (shared_from_this ());
    auto callback = [rpc_l](boost::optional<uint64_t> const & work_a) {
        if (work_a)
        {
            boost::property_tree::ptree response_l;
            response_l.put ("work", germ::to_string_hex (work_a.value ()));
            rpc_l->response (response_l);
        }
        else
        {
            error_response (rpc_l->response, "Cancelled");
        }
    };
    if (!use_peers)
    {
        node.work.generate (hash, callback);
    }
    else
    {
        node.work_generate (hash, callback);
    }
}

void germ::rpc_handler::work_cancel ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string hash_text (request.get<std::string> ("hash"));
    germ::block_hash hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad block hash");
        return;
    }

    node.work.cancel (hash);
    boost::property_tree::ptree response_l;
    response (response_l);
}

void germ::rpc_handler::work_get ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error_acc (account.decode_account (account_text));
    if (error_acc)
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, false);
    auto account_check (existing->second->store.find (transaction, account));
    if (account_check == existing->second->store.end ())
    {
        error_response (response, "Account not found in wallet");
        return;
    }

    uint64_t work (0);
    auto error_work (existing->second->store.work_get (transaction, account, work));
    boost::property_tree::ptree response_l;
    response_l.put ("work", germ::to_string_hex (work));
    response (response_l);
}

void germ::rpc_handler::work_set ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    std::string wallet_text (request.get<std::string> ("wallet"));
    germ::uint256_union wallet;
    auto error (wallet.decode_hex (wallet_text));
    if (error)
    {
        error_response (response, "Bad wallet number");
        return;
    }

    auto existing (node.wallets.items.find (wallet));
    if (existing == node.wallets.items.end ())
    {
        error_response (response, "Wallet not found");
        return;
    }

    std::string account_text (request.get<std::string> ("account"));
    germ::account account;
    auto error_acc (account.decode_account (account_text));
    if (error_acc)
    {
        error_response (response, "Bad account number");
        return;
    }

    germ::transaction transaction (node.store.environment, nullptr, true);
    auto account_check (existing->second->store.find (transaction, account));
    if (account_check == existing->second->store.end ())
    {
        error_response (response, "Account not found in wallet");
        return;
    }

    std::string work_text (request.get<std::string> ("work"));
    uint64_t work;
    auto work_error (germ::from_string_hex (work_text, work));
    if (work_error)
    {
        error_response (response, "Bad work");
        return;
    }

    existing->second->store.work_put (transaction, account, work);
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::work_validate ()
{
    std::string hash_text (request.get<std::string> ("hash"));
    germ::block_hash hash;
    auto error (hash.decode_hex (hash_text));
    if (error)
    {
        error_response (response, "Bad block hash");
        return;
    }

    std::string work_text (request.get<std::string> ("work"));
    uint64_t work;
    auto work_error (germ::from_string_hex (work_text, work));
    if (work_error)
    {
        error_response (response, "Bad work");
        return;
    }

    auto validate (!germ::work_validate (hash, work));
    boost::property_tree::ptree response_l;
    response_l.put ("valid", validate ? "0" : "1");
    response (response_l);
}

void germ::rpc_handler::work_peer_add ()
{
    if (!rpc.config.enable_control)
    {
        error_response(response, "RPC control is disabled");
        return;
    }

    std::string address_text = request.get<std::string> ("address");
    std::string port_text = request.get<std::string> ("port");
    uint16_t port;
    if (germ::parse_port (port_text, port))
    {
        error_response (response, "Invalid port");
        return;
    }

    node.config.work_peers.push_back (std::make_pair (address_text, port));
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

void germ::rpc_handler::work_peers ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    boost::property_tree::ptree work_peers_l;
    for (auto i (node.config.work_peers.begin ()), n (node.config.work_peers.end ()); i != n; ++i)
    {
        boost::property_tree::ptree entry;
        entry.put ("", boost::str (boost::format ("%1%:%2%") % i->first % i->second));
        work_peers_l.push_back (std::make_pair ("", entry));
    }
    boost::property_tree::ptree response_l;
    response_l.add_child ("work_peers", work_peers_l);
    response (response_l);
}

void germ::rpc_handler::work_peers_clear ()
{
    if (!rpc.config.enable_control)
    {
        error_response (response, "RPC control is disabled");
        return;
    }

    node.config.work_peers.clear ();
    boost::property_tree::ptree response_l;
    response_l.put ("success", "");
    response (response_l);
}

germ::rpc_connection::rpc_connection (germ::node & node_a, germ::rpc & rpc_a) :
node (node_a.shared ()),
rpc (rpc_a),
socket (node_a.service)
{
    responded.clear ();
}

void germ::rpc_connection::parse_connection ()
{
    read ();
}

void germ::rpc_connection::write_result (std::string body, unsigned version)
{
    if (!responded.test_and_set ())
    {
        res.set ("Content-Type", "application/json");
        res.set ("Access-Control-Allow-Origin", "*");
        res.set ("Access-Control-Allow-Headers", "Accept, Accept-Language, Content-Language, Content-Type");
        res.set ("Connection", "close");
        res.result (boost::beast::http::status::ok);
        res.body () = body;
        res.version (version);
        res.prepare_payload ();
    }
    else
    {
        assert (false && "RPC already responded and should only respond once");
        // Guards `res' from being clobbered while async_write is being serviced
    }
}

void germ::rpc_connection::read ()
{
    auto this_l (shared_from_this ());
    boost::beast::http::async_read (socket, buffer, request, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
        if (ec)
        {
            BOOST_LOG (this_l->node->log) << "RPC read error: " << ec.message ();
            return;
        }

        this_l->node->background ([this_l]() {
            auto start (std::chrono::steady_clock::now ());
            auto version (this_l->request.version ());
            auto response_handler ([this_l, version, start](boost::property_tree::ptree const & tree_a) {

                std::stringstream ostream;
                boost::property_tree::write_json (ostream, tree_a);
                ostream.flush ();
                auto body (ostream.str ());
                this_l->write_result (body, version);
                boost::beast::http::async_write (this_l->socket, this_l->res, [this_l](boost::system::error_code const & ec, size_t bytes_transferred) {
                });

                if (this_l->node->config.logging.log_rpc ())
                {
                    BOOST_LOG (this_l->node->log) << boost::str (boost::format ("RPC request %2% completed in: %1% microseconds") % std::chrono::duration_cast<std::chrono::microseconds> (std::chrono::steady_clock::now () - start).count () % boost::io::group (std::hex, std::showbase, reinterpret_cast<uintptr_t> (this_l.get ())));
                }
            });
            if (this_l->request.method () == boost::beast::http::verb::post)
            {
                auto handler (std::make_shared<germ::rpc_handler> (*this_l->node, this_l->rpc, this_l->request.body (), response_handler));
                handler->process_request ();
            }
            else
            {
                error_response (response_handler, "Can only POST requests");
            }
        });
    });
}

namespace
{
void reprocess_body (std::string & body, boost::property_tree::ptree & tree_a)
{
    std::stringstream stream;
    boost::property_tree::write_json (stream, tree_a);
    body = stream.str ();
}
}

void germ::rpc_handler::process_request ()
{
    try
    {
        std::stringstream istream (body);
        boost::property_tree::read_json (istream, request);
        std::string action (request.get<std::string> ("action"));
        if (action == "password_enter")
        {
            password_enter ();
            request.erase ("password");
            reprocess_body (body, request);
        }
        else if (action == "password_change")
        {
            password_change ();
            request.erase ("password");
            reprocess_body (body, request);
        }
        else if (action == "wallet_unlock")
        {
            password_enter ();
            request.erase ("password");
            reprocess_body (body, request);
        }
        if (node.config.logging.log_rpc ())
        {
            BOOST_LOG (node.log) << body;
        }
        if (action == "account_balance")
        {
            account_balance ();
        }
        else if (action == "account_block_count")
        {
            account_block_count ();
        }
        else if (action == "account_count")
        {
            account_count ();
        }
        else if (action == "account_create")
        {
            account_create ();
        }
        else if (action == "account_get")
        {
            account_get ();
        }
        else if (action == "account_history")
        {
            account_history ();
        }
        else if (action == "account_info")
        {
            account_info ();
        }
        else if (action == "account_key")
        {
            account_key ();
        }
        else if (action == "account_list")
        {
            account_list ();
        }
        else if (action == "account_move")
        {
            account_move ();
        }
        else if (action == "account_remove")
        {
            account_remove ();
        }
        else if (action == "account_representative")
        {
            account_representative ();
        }
        else if (action == "account_representative_set")
        {
            account_representative_set ();
        }
        else if (action == "account_weight")
        {
            account_weight ();
        }
        else if (action == "accounts_balances")
        {
            accounts_balances ();
        }
        else if (action == "accounts_create")
        {
            accounts_create ();
        }
        else if (action == "accounts_frontiers")
        {
            accounts_frontiers ();
        }
        else if (action == "accounts_pending")
        {
            accounts_pending ();
        }
        else if (action == "available_supply")
        {
            available_supply ();
        }
        else if (action == "block")
        {
            block ();
        }
        else if (action == "block_confirm")
        {
            block_confirm ();
        }
        else if (action == "blocks")
        {
            blocks ();
        }
        else if (action == "blocks_info")
        {
            blocks_info ();
        }
        else if (action == "block_account")
        {
            block_account ();
        }
        else if (action == "block_count")
        {
            block_count ();
        }
        else if (action == "block_count_type")
        {
            block_count_type ();
        }
        else if (action == "block_create")
        {
            block_create ();
        }
        else if (action == "block_hash")
        {
            block_hash ();
        }
        else if (action == "successors")
        {
            successors ();
        }
        else if (action == "bootstrap")
        {
            bootstrap ();
        }
        else if (action == "bootstrap_any")
        {
            bootstrap_any ();
        }
        else if (action == "chain")
        {
            chain ();
        }
        else if (action == "delegators")
        {
            delegators ();
        }
        else if (action == "delegators_count")
        {
            delegators_count ();
        }
        else if (action == "deterministic_key")
        {
            deterministic_key ();
        }
        else if (action == "confirmation_history")
        {
            confirmation_history ();
        }
        else if (action == "frontiers")
        {
            frontiers ();
        }
        else if (action == "frontier_count")
        {
            account_count ();
        }
        else if (action == "history")
        {
            request.put ("head", request.get<std::string> ("hash"));
            account_history ();
        }
        else if (action == "keepalive")
        {
            keepalive ();
        }
        else if (action == "key_create")
        {
            key_create ();
        }
        else if (action == "key_expand")
        {
            key_expand ();
        }
        else if (action == "krai_from_raw")
        {
            krai_from_raw ();
        }
        else if (action == "krai_to_raw")
        {
            krai_to_raw ();
        }
        else if (action == "ledger")
        {
            ledger ();
        }
        else if (action == "mrai_from_raw")
        {
            mrai_from_raw ();
        }
        else if (action == "mrai_to_raw")
        {
            mrai_to_raw ();
        }
        else if (action == "password_change")
        {
            // Processed before logging
        }
        else if (action == "password_enter")
        {
            // Processed before logging
        }
        else if (action == "password_valid")
        {
            password_valid ();
        }
        else if (action == "payment_begin")
        {
            payment_begin ();
        }
        else if (action == "payment_init")
        {
            payment_init ();
        }
        else if (action == "payment_end")
        {
            payment_end ();
        }
        else if (action == "payment_wait")
        {
            payment_wait ();
        }
        else if (action == "peers")
        {
            peers ();
        }
        else if (action == "pending")
        {
            pending ();
        }
        else if (action == "pending_exists")
        {
            pending_exists ();
        }
        else if (action == "process")
        {
            process ();
        }
        else if (action == "rai_from_raw")
        {
            rai_from_raw ();
        }
        else if (action == "rai_to_raw")
        {
            rai_to_raw ();
        }
        else if (action == "receive")
        {
            receive ();
        }
        else if (action == "receive_minimum")
        {
            receive_minimum ();
        }
        else if (action == "receive_minimum_set")
        {
            receive_minimum_set ();
        }
        else if (action == "representatives")
        {
            representatives ();
        }
        else if (action == "representatives_online")
        {
            representatives_online ();
        }
        else if (action == "republish")
        {
            republish ();
        }
        else if (action == "search_pending")
        {
            search_pending ();
        }
        else if (action == "search_pending_all")
        {
            search_pending_all ();
        }
        else if (action == "send")
        {
            send ();
        }
        else if (action == "stats")
        {
            stats ();
        }
        else if (action == "stop")
        {
            stop ();
        }
        else if (action == "unchecked")
        {
            unchecked ();
        }
        else if (action == "unchecked_clear")
        {
            unchecked_clear ();
        }
        else if (action == "unchecked_get")
        {
            unchecked_get ();
        }
        else if (action == "unchecked_keys")
        {
            unchecked_keys ();
        }
        else if (action == "validate_account_number")
        {
            validate_account_number ();
        }
        else if (action == "version")
        {
            version ();
        }
        else if (action == "wallet_add")
        {
            wallet_add ();
        }
        else if (action == "wallet_add_watch")
        {
            wallet_add_watch ();
        }
        else if (action == "wallet_balance_total")
        {
            wallet_balance_total ();
        }
        else if (action == "wallet_balances")
        {
            wallet_balances ();
        }
        else if (action == "wallet_change_seed")
        {
            wallet_change_seed ();
        }
        else if (action == "wallet_contains")
        {
            wallet_contains ();
        }
        else if (action == "wallet_create")
        {
            wallet_create ();
        }
        else if (action == "wallet_destroy")
        {
            wallet_destroy ();
        }
        else if (action == "wallet_export")
        {
            wallet_export ();
        }
        else if (action == "wallet_frontiers")
        {
            wallet_frontiers ();
        }
        else if (action == "wallet_key_valid")
        {
            wallet_key_valid ();
        }
        else if (action == "wallet_ledger")
        {
            wallet_ledger ();
        }
        else if (action == "wallet_lock")
        {
            wallet_lock ();
        }
        else if (action == "wallet_locked")
        {
            password_valid (true);
        }
        else if (action == "wallet_pending")
        {
            wallet_pending ();
        }
        else if (action == "wallet_representative")
        {
            wallet_representative ();
        }
        else if (action == "wallet_representative_set")
        {
            wallet_representative_set ();
        }
        else if (action == "wallet_republish")
        {
            wallet_republish ();
        }
        else if (action == "wallet_unlock")
        {
            // Processed before logging
        }
        else if (action == "wallet_work_get")
        {
            wallet_work_get ();
        }
        else if (action == "work_generate")
        {
            work_generate ();
        }
        else if (action == "work_cancel")
        {
            work_cancel ();
        }
        else if (action == "work_get")
        {
            work_get ();
        }
        else if (action == "work_set")
        {
            work_set ();
        }
        else if (action == "work_validate")
        {
            work_validate ();
        }
        else if (action == "work_peer_add")
        {
            work_peer_add ();
        }
        else if (action == "work_peers")
        {
            work_peers ();
        }
        else if (action == "work_peers_clear")
        {
            work_peers_clear ();
        }
        else
        {
            error_response (response, "Unknown command");
        }
    }
    catch (std::runtime_error const & err)
    {
        error_response (response, "Unable to parse JSON");
    }
    catch (...)
    {
        error_response (response, "Internal server error in RPC");
    }
}

germ::payment_observer::payment_observer (std::function<void(boost::property_tree::ptree const &)> const & response_a, germ::rpc & rpc_a, germ::account const & account_a, germ::amount const & amount_a) :
rpc (rpc_a),
account (account_a),
amount (amount_a),
response (response_a)
{
    completed.clear ();
}

void germ::payment_observer::start (uint64_t timeout)
{
    auto this_l (shared_from_this ());
    rpc.node.alarm.add (std::chrono::steady_clock::now () + std::chrono::milliseconds (timeout), [this_l]() {
        this_l->complete (germ::payment_status::nothing);
    });
}

germ::payment_observer::~payment_observer ()
{
}

void germ::payment_observer::observe ()
{
    if (rpc.node.balance (account) >= amount.number ())
    {
        complete (germ::payment_status::success);
    }
}

void germ::payment_observer::complete (germ::payment_status status)
{
    auto already (completed.test_and_set ());
    if (already)
        return;

    if (rpc.node.config.logging.log_rpc ())
    {
        BOOST_LOG (rpc.node.log) << boost::str (boost::format ("Exiting payment_observer for account %1% status %2%") % account.to_account () % static_cast<unsigned> (status));
    }
    switch (status)
    {
        case germ::payment_status::nothing:
        {
            boost::property_tree::ptree response_l;
            response_l.put ("status", "nothing");
            response (response_l);
            break;
        }
        case germ::payment_status::success:
        {
            boost::property_tree::ptree response_l;
            response_l.put ("status", "success");
            response (response_l);
            break;
        }
        default:
        {
            error_response (response, "Internal payment error");
            break;
        }
    }
    std::lock_guard<std::mutex> lock (rpc.mutex);
    assert (rpc.payment_observers.find (account) != rpc.payment_observers.end ());
    rpc.payment_observers.erase (account);
}

std::unique_ptr<germ::rpc> germ::get_rpc (boost::asio::io_service & service_a, germ::node & node_a, germ::rpc_config const & config_a)
{
    std::unique_ptr<rpc> impl;

    if (config_a.secure.enable)
    {
#ifdef GERMBLOCKS_SECURE_RPC
        impl.reset (new rpc_secure (service_a, node_a, config_a));
#else
        std::cerr << "RPC configured for TLS, but the node is not compiled with TLS support" << std::endl;
#endif
    }
    else
    {
        impl.reset (new rpc (service_a, node_a, config_a));
    }

    return impl;
}
