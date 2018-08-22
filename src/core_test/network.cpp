#include <boost/thread.hpp>
#include <gtest/gtest.h>
#include <src/node/testing.hpp>

TEST (network, tcp_connection)
{
	boost::asio::io_service service;
	boost::asio::ip::tcp::acceptor acceptor (service);
	boost::asio::ip::tcp::endpoint endpoint (boost::asio::ip::address_v4::any (), 24000);
	acceptor.open (endpoint.protocol ());
	acceptor.set_option (boost::asio::ip::tcp::acceptor::reuse_address (true));
	acceptor.bind (endpoint);
	acceptor.listen ();
	boost::asio::ip::tcp::socket incoming (service);
	auto done1 (false);
	std::string message1;
	acceptor.async_accept (incoming,
	[&done1, &message1](boost::system::error_code const & ec_a) {
		   if (ec_a)
		   {
			   message1 = ec_a.message ();
			   std::cerr << message1;
		   }
		   done1 = true; });
	boost::asio::ip::tcp::socket connector (service);
	auto done2 (false);
	std::string message2;
	connector.async_connect (boost::asio::ip::tcp::endpoint (boost::asio::ip::address_v4::loopback (), 24000),
	[&done2, &message2](boost::system::error_code const & ec_a) {
		if (ec_a)
		{
			message2 = ec_a.message ();
			std::cerr << message2;
		}
		done2 = true;
	});
	while (!done1 || !done2)
	{
		service.poll ();
	}
	ASSERT_EQ (0, message1.size ());
	ASSERT_EQ (0, message2.size ());
}

TEST (network, construction)
{
	germ::system system (24000, 1);
	ASSERT_EQ (1, system.nodes.size ());
	ASSERT_EQ (24000, system.nodes[0]->network.socket.local_endpoint ().port ());
}

TEST (network, self_discard)
{
	germ::system system (24000, 1);
	system.nodes[0]->network.remote = system.nodes[0]->network.endpoint ();
	ASSERT_EQ (0, system.nodes[0]->stats.count (germ::stat::type::error, germ::stat::detail::bad_sender));
	system.nodes[0]->network.receive_action (boost::system::error_code{}, 0);
	ASSERT_EQ (1, system.nodes[0]->stats.count (germ::stat::type::error, germ::stat::detail::bad_sender));
}

TEST (network, send_node_id_handshake)
{
	germ::system system (24000, 1);
	auto list1 (system.nodes[0]->peers.list ());
	ASSERT_EQ (0, list1.size ());
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	node1->start ();
	auto initial (system.nodes[0]->stats.count (germ::stat::type::message, germ::stat::detail::node_id_handshake, germ::stat::dir::in));
	auto initial_node1 (node1->stats.count (germ::stat::type::message, germ::stat::detail::node_id_handshake, germ::stat::dir::in));
	system.nodes[0]->network.send_keepalive (node1->network.endpoint ());
	ASSERT_EQ (0, system.nodes[0]->peers.list ().size ());
	ASSERT_EQ (0, node1->peers.list ().size ());
	auto iterations (0);
	while (node1->stats.count (germ::stat::type::message, germ::stat::detail::node_id_handshake, germ::stat::dir::in) == initial_node1)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (0, system.nodes[0]->peers.list ().size ());
	ASSERT_EQ (1, node1->peers.list ().size ());
	iterations = 0;
	while (system.nodes[0]->stats.count (germ::stat::type::message, germ::stat::detail::node_id_handshake, germ::stat::dir::in) < initial + 2)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	auto peers1 (system.nodes[0]->peers.list ());
	auto peers2 (node1->peers.list ());
	ASSERT_EQ (1, peers1.size ());
	ASSERT_EQ (1, peers2.size ());
	ASSERT_EQ (node1->network.endpoint (), peers1[0]);
	ASSERT_EQ (system.nodes[0]->network.endpoint (), peers2[0]);
	node1->stop ();
}

TEST (network, keepalive_ipv4)
{
	germ::system system (24000, 1);
	auto list1 (system.nodes[0]->peers.list ());
	ASSERT_EQ (0, list1.size ());
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	node1->start ();
	node1->send_keepalive (germ::endpoint (boost::asio::ip::address_v4::loopback (), 24000));
	auto initial (system.nodes[0]->stats.count (germ::stat::type::message, germ::stat::detail::keepalive, germ::stat::dir::in));
	auto iterations (0);
	while (system.nodes[0]->stats.count (germ::stat::type::message, germ::stat::detail::keepalive, germ::stat::dir::in) == initial)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	node1->stop ();
}

TEST (network, multi_keepalive)
{
	germ::system system (24000, 1);
	auto list1 (system.nodes[0]->peers.list ());
	ASSERT_EQ (0, list1.size ());
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	node1->start ();
	ASSERT_EQ (0, node1->peers.size ());
	node1->network.send_keepalive (system.nodes[0]->network.endpoint ());
	ASSERT_EQ (0, node1->peers.size ());
	ASSERT_EQ (0, system.nodes[0]->peers.size ());
	auto iterations1 (0);
	while (system.nodes[0]->peers.size () != 1)
	{
		system.poll ();
		++iterations1;
		ASSERT_LT (iterations1, 200);
	}
	germ::node_init init2;
	auto node2 (std::make_shared<germ::node> (init2, system.service, 24002, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init2.error ());
	node2->start ();
	node2->network.send_keepalive (system.nodes[0]->network.endpoint ());
	auto iterations2 (0);
	while (node1->peers.size () != 2 || system.nodes[0]->peers.size () != 2 || node2->peers.size () != 2)
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 200);
	}
	node1->stop ();
	node2->stop ();
}

TEST (network, send_discarded_publish)
{
	germ::system system (24000, 2);
	auto block (std::make_shared<germ::send_block> (1, 1, 2, germ::keypair ().prv, 4, system.work.generate (1)));
	germ::genesis genesis;
	{
		germ::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		system.nodes[0]->network.republish_block (transaction, block);
		ASSERT_EQ (genesis.hash (), system.nodes[0]->ledger.latest (transaction, germ::test_genesis_key.pub));
		ASSERT_EQ (genesis.hash (), system.nodes[1]->latest (germ::test_genesis_key.pub));
	}
	auto iterations (0);
	while (system.nodes[1]->stats.count (germ::stat::type::message, germ::stat::detail::publish, germ::stat::dir::in) == 0)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	germ::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_EQ (genesis.hash (), system.nodes[0]->ledger.latest (transaction, germ::test_genesis_key.pub));
	ASSERT_EQ (genesis.hash (), system.nodes[1]->latest (germ::test_genesis_key.pub));
}

TEST (network, send_invalid_publish)
{
	germ::system system (24000, 2);
	germ::genesis genesis;
	auto block (std::make_shared<germ::send_block> (1, 1, 20, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (1)));
	{
		germ::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
		system.nodes[0]->network.republish_block (transaction, block);
		ASSERT_EQ (genesis.hash (), system.nodes[0]->ledger.latest (transaction, germ::test_genesis_key.pub));
		ASSERT_EQ (genesis.hash (), system.nodes[1]->latest (germ::test_genesis_key.pub));
	}
	auto iterations (0);
	while (system.nodes[1]->stats.count (germ::stat::type::message, germ::stat::detail::publish, germ::stat::dir::in) == 0)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	germ::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_EQ (genesis.hash (), system.nodes[0]->ledger.latest (transaction, germ::test_genesis_key.pub));
	ASSERT_EQ (genesis.hash (), system.nodes[1]->latest (germ::test_genesis_key.pub));
}

TEST (network, send_valid_confirm_ack)
{
	germ::system system (24000, 2);
	germ::keypair key2;
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	system.wallet (1)->insert_adhoc (key2.prv);
	germ::block_hash latest1 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	germ::send_block block2 (latest1, key2.pub, 50, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (latest1));
	germ::block_hash latest2 (system.nodes[1]->latest (germ::test_genesis_key.pub));
	system.nodes[0]->process_active (std::unique_ptr<germ::tx> (new germ::send_block (block2)));
	auto iterations (0);
	// Keep polling until latest block changes
	while (system.nodes[1]->latest (germ::test_genesis_key.pub) == latest2)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	// Make sure the balance has decreased after processing the block.
	ASSERT_EQ (50, system.nodes[1]->balance (germ::test_genesis_key.pub));
}

TEST (network, send_valid_publish)
{
	germ::system system (24000, 2);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::keypair key2;
	system.wallet (1)->insert_adhoc (key2.prv);
	germ::block_hash latest1 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	germ::send_block block2 (latest1, key2.pub, 50, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (latest1));
	auto hash2 (block2.hash ());
	germ::block_hash latest2 (system.nodes[1]->latest (germ::test_genesis_key.pub));
	system.nodes[1]->process_active (std::unique_ptr<germ::tx> (new germ::send_block (block2)));
	auto iterations (0);
	while (system.nodes[0]->stats.count (germ::stat::type::message, germ::stat::detail::publish, germ::stat::dir::in) == 0)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	germ::block_hash latest3 (system.nodes[1]->latest (germ::test_genesis_key.pub));
	ASSERT_NE (latest2, latest3);
	ASSERT_EQ (hash2, latest3);
	ASSERT_EQ (50, system.nodes[1]->balance (germ::test_genesis_key.pub));
}

TEST (network, send_insufficient_work)
{
	germ::system system (24000, 2);
	std::unique_ptr<germ::send_block> block (new germ::send_block (0, 1, 20, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	germ::publish publish (std::move (block));
	std::shared_ptr<std::vector<uint8_t>> bytes (new std::vector<uint8_t>);
	{
		germ::vectorstream stream (*bytes);
		publish.serialize (stream);
	}
	auto node1 (system.nodes[1]->shared ());
	system.nodes[0]->network.send_buffer (bytes->data (), bytes->size (), system.nodes[1]->network.endpoint (), [bytes, node1](boost::system::error_code const & ec, size_t size) {});
	ASSERT_EQ (0, system.nodes[0]->stats.count (germ::stat::type::error, germ::stat::detail::insufficient_work));
	auto iterations (0);
	while (system.nodes[1]->stats.count (germ::stat::type::error, germ::stat::detail::insufficient_work) == 0)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (1, system.nodes[1]->stats.count (germ::stat::type::error, germ::stat::detail::insufficient_work));
}

TEST (receivable_processor, confirm_insufficient_pos)
{
	germ::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	germ::genesis genesis;
	auto block1 (std::make_shared<germ::send_block> (genesis.hash (), 0, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*block1);
	ASSERT_EQ (germ::process_result::progress, node1.process (*block1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (block1);
	germ::keypair key1;
	auto vote (std::make_shared<germ::vote> (key1.pub, key1.prv, 0, block1));
	germ::confirm_ack con1 (vote);
	node1.process_message (con1, node1.network.endpoint ());
}

TEST (receivable_processor, confirm_sufficient_pos)
{
	germ::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	germ::genesis genesis;
	auto block1 (std::make_shared<germ::send_block> (genesis.hash (), 0, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	node1.work_generate_blocking (*block1);
	ASSERT_EQ (germ::process_result::progress, node1.process (*block1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (block1);
	auto vote (std::make_shared<germ::vote> (germ::test_genesis_key.pub, germ::test_genesis_key.prv, 0, block1));
	germ::confirm_ack con1 (vote);
	node1.process_message (con1, node1.network.endpoint ());
}

TEST (receivable_processor, send_with_receive)
{
	auto amount (std::numeric_limits<germ::uint128_t>::max ());
	germ::system system (24000, 2);
	germ::keypair key2;
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::block_hash latest1 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	system.wallet (1)->insert_adhoc (key2.prv);
	auto block1 (std::make_shared<germ::send_block> (latest1, key2.pub, amount - system.nodes[0]->config.receive_minimum.number (), germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (latest1)));
	ASSERT_EQ (amount, system.nodes[0]->balance (germ::test_genesis_key.pub));
	ASSERT_EQ (0, system.nodes[0]->balance (key2.pub));
	ASSERT_EQ (amount, system.nodes[1]->balance (germ::test_genesis_key.pub));
	ASSERT_EQ (0, system.nodes[1]->balance (key2.pub));
	system.nodes[0]->process_active (block1);
	system.nodes[0]->block_processor.flush ();
	system.nodes[1]->process_active (block1);
	system.nodes[1]->block_processor.flush ();
	ASSERT_EQ (amount - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (germ::test_genesis_key.pub));
	ASSERT_EQ (0, system.nodes[0]->balance (key2.pub));
	ASSERT_EQ (amount - system.nodes[0]->config.receive_minimum.number (), system.nodes[1]->balance (germ::test_genesis_key.pub));
	ASSERT_EQ (0, system.nodes[1]->balance (key2.pub));
	auto iterations (0);
	while (system.nodes[0]->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number ())
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (amount - system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (germ::test_genesis_key.pub));
	ASSERT_EQ (system.nodes[0]->config.receive_minimum.number (), system.nodes[0]->balance (key2.pub));
	ASSERT_EQ (amount - system.nodes[0]->config.receive_minimum.number (), system.nodes[1]->balance (germ::test_genesis_key.pub));
	ASSERT_EQ (system.nodes[0]->config.receive_minimum.number (), system.nodes[1]->balance (key2.pub));
}

TEST (network, receive_weight_change)
{
	germ::system system (24000, 2);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::keypair key2;
	system.wallet (1)->insert_adhoc (key2.prv);
	{
		germ::transaction transaction (system.nodes[1]->store.environment, nullptr, true);
		system.wallet (1)->store.representative_set (transaction, key2.pub);
	}
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	auto iterations (0);
	while (std::any_of (system.nodes.begin (), system.nodes.end (), [&](std::shared_ptr<germ::node> const & node_a) { return node_a->weight (key2.pub) != system.nodes[0]->config.receive_minimum.number (); }))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
}

TEST (parse_endpoint, valid)
{
	std::string string ("::1:24000");
	germ::endpoint endpoint;
	ASSERT_FALSE (germ::parse_endpoint (string, endpoint));
	ASSERT_EQ (boost::asio::ip::address_v6::loopback (), endpoint.address ());
	ASSERT_EQ (24000, endpoint.port ());
}

TEST (parse_endpoint, invalid_port)
{
	std::string string ("::1:24a00");
	germ::endpoint endpoint;
	ASSERT_TRUE (germ::parse_endpoint (string, endpoint));
}

TEST (parse_endpoint, invalid_address)
{
	std::string string ("::q:24000");
	germ::endpoint endpoint;
	ASSERT_TRUE (germ::parse_endpoint (string, endpoint));
}

TEST (parse_endpoint, no_address)
{
	std::string string (":24000");
	germ::endpoint endpoint;
	ASSERT_TRUE (germ::parse_endpoint (string, endpoint));
}

TEST (parse_endpoint, no_port)
{
	std::string string ("::1:");
	germ::endpoint endpoint;
	ASSERT_TRUE (germ::parse_endpoint (string, endpoint));
}

TEST (parse_endpoint, no_colon)
{
	std::string string ("::1");
	germ::endpoint endpoint;
	ASSERT_TRUE (germ::parse_endpoint (string, endpoint));
}

// If the account doesn't exist, current == end so there's no iteration
TEST (bulk_pull, no_address)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::bulk_pull> req (new germ::bulk_pull);
	req->start = 1;
	req->end = 2;
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::bulk_pull_server> (connection, std::move (req)));
	ASSERT_EQ (request->current, request->request->end);
	ASSERT_TRUE (request->current.is_zero ());
}

TEST (bulk_pull, genesis_to_end)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::bulk_pull> req (new germ::bulk_pull{});
	req->start = germ::test_genesis_key.pub;
	req->end.clear ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::bulk_pull_server> (connection, std::move (req)));
	ASSERT_EQ (system.nodes[0]->latest (germ::test_genesis_key.pub), request->current);
	ASSERT_EQ (request->request->end, request->request->end);
}

// If we can't find the end block, send everything
TEST (bulk_pull, no_end)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::bulk_pull> req (new germ::bulk_pull{});
	req->start = germ::test_genesis_key.pub;
	req->end = 1;
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::bulk_pull_server> (connection, std::move (req)));
	ASSERT_EQ (system.nodes[0]->latest (germ::test_genesis_key.pub), request->current);
	ASSERT_TRUE (request->request->end.is_zero ());
}

TEST (bulk_pull, end_not_owned)
{
	germ::system system (24000, 1);
	germ::keypair key2;
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, key2.pub, 100));
	germ::block_hash latest (system.nodes[0]->latest (germ::test_genesis_key.pub));
	germ::open_block open (0, 1, 2, germ::keypair ().prv, 4, 5);
	open.hashables.account = key2.pub;
	open.hashables.representative = key2.pub;
	open.hashables.source = latest;
	open.signature = germ::sign_message (key2.prv, key2.pub, open.hash ());
	ASSERT_EQ (germ::process_result::progress, system.nodes[0]->process (open).code);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	germ::genesis genesis;
	std::unique_ptr<germ::bulk_pull> req (new germ::bulk_pull{});
	req->start = key2.pub;
	req->end = genesis.hash ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::bulk_pull_server> (connection, std::move (req)));
	ASSERT_EQ (request->current, request->request->end);
}

TEST (bulk_pull, none)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	germ::genesis genesis;
	std::unique_ptr<germ::bulk_pull> req (new germ::bulk_pull{});
	req->start = genesis.hash ();
	req->end = genesis.hash ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::bulk_pull_server> (connection, std::move (req)));
	auto block (request->get_next ());
	ASSERT_EQ (nullptr, block);
}

TEST (bulk_pull, get_next_on_open)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::bulk_pull> req (new germ::bulk_pull{});
	req->start = germ::test_genesis_key.pub;
	req->end.clear ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::bulk_pull_server> (connection, std::move (req)));
	auto block (request->get_next ());
	ASSERT_NE (nullptr, block);
	ASSERT_TRUE (block->previous ().is_zero ());
	ASSERT_FALSE (connection->requests.empty ());
	ASSERT_EQ (request->current, request->request->end);
}

TEST (bootstrap_processor, DISABLED_process_none)
{
	germ::system system (24000, 1);
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	auto done (false);
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	while (!done)
	{
		system.service.run_one ();
	}
	node1->stop ();
}

// Bootstrap can pull one basic block
TEST (bootstrap_processor, process_one)
{
	germ::system system (24000, 1);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, germ::test_genesis_key.pub, 100));
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	germ::block_hash hash1 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	germ::block_hash hash2 (node1->latest (germ::test_genesis_key.pub));
	ASSERT_NE (hash1, hash2);
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations (0);
	ASSERT_NE (node1->latest (germ::test_genesis_key.pub), system.nodes[0]->latest (germ::test_genesis_key.pub));
	while (node1->latest (germ::test_genesis_key.pub) != system.nodes[0]->latest (germ::test_genesis_key.pub))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (0, node1->active.roots.size ());
	node1->stop ();
}

TEST (bootstrap_processor, process_two)
{
	germ::system system (24000, 1);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::block_hash hash1 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, germ::test_genesis_key.pub, 50));
	germ::block_hash hash2 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, germ::test_genesis_key.pub, 50));
	germ::block_hash hash3 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	ASSERT_NE (hash1, hash2);
	ASSERT_NE (hash1, hash3);
	ASSERT_NE (hash2, hash3);
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations (0);
	ASSERT_NE (node1->latest (germ::test_genesis_key.pub), system.nodes[0]->latest (germ::test_genesis_key.pub));
	while (node1->latest (germ::test_genesis_key.pub) != system.nodes[0]->latest (germ::test_genesis_key.pub))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	node1->stop ();
}

// Bootstrap can pull universal blocks
TEST (bootstrap_processor, process_state)
{
	germ::system system (24000, 1);
	germ::genesis genesis;
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	auto node0 (system.nodes[0]);
	std::unique_ptr<germ::tx> block1 (new germ::state_block (germ::test_genesis_key.pub, node0->latest (germ::test_genesis_key.pub), germ::test_genesis_key.pub, germ::genesis_amount - 100, germ::test_genesis_key.pub, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	std::unique_ptr<germ::tx> block2 (new germ::state_block (germ::test_genesis_key.pub, block1->hash (), germ::test_genesis_key.pub, germ::genesis_amount, block1->hash (), germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	node0->work_generate_blocking (*block1);
	node0->work_generate_blocking (*block2);
	node0->process (*block1);
	node0->process (*block2);
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_EQ (node0->latest (germ::test_genesis_key.pub), block2->hash ());
	ASSERT_NE (node1->latest (germ::test_genesis_key.pub), block2->hash ());
	node1->bootstrap_initiator.bootstrap (node0->network.endpoint ());
	auto iterations (0);
	ASSERT_NE (node1->latest (germ::test_genesis_key.pub), node0->latest (germ::test_genesis_key.pub));
	while (node1->latest (germ::test_genesis_key.pub) != node0->latest (germ::test_genesis_key.pub))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (0, node1->active.roots.size ());
	node1->stop ();
}

TEST (bootstrap_processor, process_new)
{
	germ::system system (24000, 2);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::keypair key2;
	system.wallet (1)->insert_adhoc (key2.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	auto iterations1 (0);
	while (system.nodes[0]->balance (key2.pub).is_zero ())
	{
		system.poll ();
		++iterations1;
		ASSERT_LT (iterations1, 200);
	}
	germ::uint128_t balance1 (system.nodes[0]->balance (germ::test_genesis_key.pub));
	germ::uint128_t balance2 (system.nodes[0]->balance (key2.pub));
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24002, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations2 (0);
	while (node1->balance (key2.pub) != balance2)
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 200);
	}
	ASSERT_EQ (balance1, node1->balance (germ::test_genesis_key.pub));
	node1->stop ();
}

TEST (bootstrap_processor, pull_diamond)
{
	germ::system system (24000, 1);
	germ::keypair key;
	std::unique_ptr<germ::send_block> send1 (new germ::send_block (system.nodes[0]->latest (germ::test_genesis_key.pub), key.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (system.nodes[0]->latest (germ::test_genesis_key.pub))));
	ASSERT_EQ (germ::process_result::progress, system.nodes[0]->process (*send1).code);
	std::unique_ptr<germ::open_block> open (new germ::open_block (send1->hash (), 1, key.pub, key.prv, key.pub, system.work.generate (key.pub)));
	ASSERT_EQ (germ::process_result::progress, system.nodes[0]->process (*open).code);
	std::unique_ptr<germ::send_block> send2 (new germ::send_block (open->hash (), germ::test_genesis_key.pub, std::numeric_limits<germ::uint128_t>::max () - 100, key.prv, key.pub, system.work.generate (open->hash ())));
	ASSERT_EQ (germ::process_result::progress, system.nodes[0]->process (*send2).code);
	std::unique_ptr<germ::receive_block> receive (new germ::receive_block (send1->hash (), send2->hash (), germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (send1->hash ())));
	ASSERT_EQ (germ::process_result::progress, system.nodes[0]->process (*receive).code);
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24002, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations (0);
	while (node1->balance (germ::test_genesis_key.pub) != 100)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (100, node1->balance (germ::test_genesis_key.pub));
	node1->stop ();
}

TEST (bootstrap_processor, push_diamond)
{
	germ::system system (24000, 1);
	germ::keypair key;
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24002, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	auto wallet1 (node1->wallets.create (100));
	wallet1->insert_adhoc (germ::test_genesis_key.prv);
	wallet1->insert_adhoc (key.prv);
	std::unique_ptr<germ::send_block> send1 (new germ::send_block (system.nodes[0]->latest (germ::test_genesis_key.pub), key.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (system.nodes[0]->latest (germ::test_genesis_key.pub))));
	ASSERT_EQ (germ::process_result::progress, node1->process (*send1).code);
	std::unique_ptr<germ::open_block> open (new germ::open_block (send1->hash (), 1, key.pub, key.prv, key.pub, system.work.generate (key.pub)));
	ASSERT_EQ (germ::process_result::progress, node1->process (*open).code);
	std::unique_ptr<germ::send_block> send2 (new germ::send_block (open->hash (), germ::test_genesis_key.pub, std::numeric_limits<germ::uint128_t>::max () - 100, key.prv, key.pub, system.work.generate (open->hash ())));
	ASSERT_EQ (germ::process_result::progress, node1->process (*send2).code);
	std::unique_ptr<germ::receive_block> receive (new germ::receive_block (send1->hash (), send2->hash (), germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (send1->hash ())));
	ASSERT_EQ (germ::process_result::progress, node1->process (*receive).code);
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations (0);
	while (system.nodes[0]->balance (germ::test_genesis_key.pub) != 100)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (100, system.nodes[0]->balance (germ::test_genesis_key.pub));
	node1->stop ();
}

TEST (bootstrap_processor, push_one)
{
	germ::system system (24000, 1);
	germ::node_init init1;
	germ::keypair key1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	auto wallet (node1->wallets.create (germ::uint256_union ()));
	ASSERT_NE (nullptr, wallet);
	wallet->insert_adhoc (germ::test_genesis_key.prv);
	germ::uint128_t balance1 (node1->balance (germ::test_genesis_key.pub));
	ASSERT_NE (nullptr, wallet->send_action (germ::test_genesis_key.pub, key1.pub, 100));
	ASSERT_NE (balance1, node1->balance (germ::test_genesis_key.pub));
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations (0);
	while (system.nodes[0]->balance (germ::test_genesis_key.pub) == balance1)
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	node1->stop ();
}

TEST (frontier_req_response, DISABLED_destruction)
{
	{
		std::shared_ptr<germ::frontier_req_server> hold; // Destructing tcp acceptor on non-existent io_service
		{
			germ::system system (24000, 1);
			auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
			std::unique_ptr<germ::frontier_req> req (new germ::frontier_req);
			req->start.clear ();
			req->age = std::numeric_limits<decltype (req->age)>::max ();
			req->count = std::numeric_limits<decltype (req->count)>::max ();
			connection->requests.push (std::unique_ptr<germ::message>{});
			hold = std::make_shared<germ::frontier_req_server> (connection, std::move (req));
		}
	}
	ASSERT_TRUE (true);
}

TEST (frontier_req, begin)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::frontier_req> req (new germ::frontier_req);
	req->start.clear ();
	req->age = std::numeric_limits<decltype (req->age)>::max ();
	req->count = std::numeric_limits<decltype (req->count)>::max ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::frontier_req_server> (connection, std::move (req)));
	ASSERT_EQ (germ::test_genesis_key.pub, request->current);
	germ::genesis genesis;
	ASSERT_EQ (genesis.hash (), request->info.head);
}

TEST (frontier_req, end)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::frontier_req> req (new germ::frontier_req);
	req->start = germ::test_genesis_key.pub.number () + 1;
	req->age = std::numeric_limits<decltype (req->age)>::max ();
	req->count = std::numeric_limits<decltype (req->count)>::max ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::frontier_req_server> (connection, std::move (req)));
	ASSERT_TRUE (request->current.is_zero ());
}

TEST (frontier_req, time_bound)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::frontier_req> req (new germ::frontier_req);
	req->start.clear ();
	req->age = 0;
	req->count = std::numeric_limits<decltype (req->count)>::max ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::frontier_req_server> (connection, std::move (req)));
	ASSERT_TRUE (request->current.is_zero ());
}

TEST (frontier_req, time_cutoff)
{
	germ::system system (24000, 1);
	auto connection (std::make_shared<germ::bootstrap_server> (nullptr, system.nodes[0]));
	std::unique_ptr<germ::frontier_req> req (new germ::frontier_req);
	req->start.clear ();
	req->age = 10;
	req->count = std::numeric_limits<decltype (req->count)>::max ();
	connection->requests.push (std::unique_ptr<germ::message>{});
	auto request (std::make_shared<germ::frontier_req_server> (connection, std::move (req)));
	ASSERT_EQ (germ::test_genesis_key.pub, request->current);
	germ::genesis genesis;
	ASSERT_EQ (genesis.hash (), request->info.head);
}

TEST (bulk, genesis)
{
	germ::system system (24000, 1);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	germ::block_hash latest1 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	germ::block_hash latest2 (node1->latest (germ::test_genesis_key.pub));
	ASSERT_EQ (latest1, latest2);
	germ::keypair key2;
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, key2.pub, 100));
	germ::block_hash latest3 (system.nodes[0]->latest (germ::test_genesis_key.pub));
	ASSERT_NE (latest1, latest3);
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations (0);
	while (node1->latest (germ::test_genesis_key.pub) != system.nodes[0]->latest (germ::test_genesis_key.pub))
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (node1->latest (germ::test_genesis_key.pub), system.nodes[0]->latest (germ::test_genesis_key.pub));
	node1->stop ();
}

TEST (bulk, offline_send)
{
	germ::system system (24000, 1);
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	germ::node_init init1;
	auto node1 (std::make_shared<germ::node> (init1, system.service, 24001, germ::unique_path (), system.alarm, system.logging, system.work));
	ASSERT_FALSE (init1.error ());
	node1->network.send_keepalive (system.nodes[0]->network.endpoint ());
	node1->start ();
	auto iterations (0);
	do
	{
		system.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	} while (system.nodes[0]->peers.empty () || node1->peers.empty ());
	germ::keypair key2;
	auto wallet (node1->wallets.create (germ::uint256_union ()));
	wallet->insert_adhoc (key2.prv);
	ASSERT_NE (nullptr, system.wallet (0)->send_action (germ::test_genesis_key.pub, key2.pub, system.nodes[0]->config.receive_minimum.number ()));
	ASSERT_NE (std::numeric_limits<germ::uint256_t>::max (), system.nodes[0]->balance (germ::test_genesis_key.pub));
	node1->bootstrap_initiator.bootstrap (system.nodes[0]->network.endpoint ());
	auto iterations2 (0);
	while (node1->balance (key2.pub) != system.nodes[0]->config.receive_minimum.number ())
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 200);
	}
	node1->stop ();
}

TEST (network, ipv6)
{
	boost::asio::ip::address_v6 address (boost::asio::ip::address_v6::from_string ("::ffff:127.0.0.1"));
	ASSERT_TRUE (address.is_v4_mapped ());
	germ::endpoint endpoint1 (address, 16384);
	std::vector<uint8_t> bytes1;
	{
		germ::vectorstream stream (bytes1);
		germ::write (stream, address.to_bytes ());
	}
	ASSERT_EQ (16, bytes1.size ());
	for (auto i (bytes1.begin ()), n (bytes1.begin () + 10); i != n; ++i)
	{
		ASSERT_EQ (0, *i);
	}
	ASSERT_EQ (0xff, bytes1[10]);
	ASSERT_EQ (0xff, bytes1[11]);
	std::array<uint8_t, 16> bytes2;
	germ::bufferstream stream (bytes1.data (), bytes1.size ());
	germ::read (stream, bytes2);
	germ::endpoint endpoint2 (boost::asio::ip::address_v6 (bytes2), 16384);
	ASSERT_EQ (endpoint1, endpoint2);
}

TEST (network, ipv6_from_ipv4)
{
	germ::endpoint endpoint1 (boost::asio::ip::address_v4::loopback (), 16000);
	ASSERT_TRUE (endpoint1.address ().is_v4 ());
	germ::endpoint endpoint2 (boost::asio::ip::address_v6::v4_mapped (endpoint1.address ().to_v4 ()), 16000);
	ASSERT_TRUE (endpoint2.address ().is_v6 ());
}

TEST (network, ipv6_bind_send_ipv4)
{
	boost::asio::io_service service;
	germ::endpoint endpoint1 (boost::asio::ip::address_v6::any (), 24000);
	germ::endpoint endpoint2 (boost::asio::ip::address_v4::any (), 24001);
	std::array<uint8_t, 16> bytes1;
	auto finish1 (false);
	germ::endpoint endpoint3;
	boost::asio::ip::udp::socket socket1 (service, endpoint1);
	socket1.async_receive_from (boost::asio::buffer (bytes1.data (), bytes1.size ()), endpoint3, [&finish1](boost::system::error_code const & error, size_t size_a) {
		ASSERT_FALSE (error);
		ASSERT_EQ (16, size_a);
		finish1 = true;
	});
	boost::asio::ip::udp::socket socket2 (service, endpoint2);
	germ::endpoint endpoint5 (boost::asio::ip::address_v4::loopback (), 24000);
	germ::endpoint endpoint6 (boost::asio::ip::address_v6::v4_mapped (boost::asio::ip::address_v4::loopback ()), 24001);
	socket2.async_send_to (boost::asio::buffer (std::array<uint8_t, 16>{}, 16), endpoint5, [](boost::system::error_code const & error, size_t size_a) {
		ASSERT_FALSE (error);
		ASSERT_EQ (16, size_a);
	});
	auto iterations (0);
	while (!finish1)
	{
		service.poll ();
		++iterations;
		ASSERT_LT (iterations, 200);
	}
	ASSERT_EQ (endpoint6, endpoint3);
	std::array<uint8_t, 16> bytes2;
	auto finish2 (false);
	germ::endpoint endpoint4;
	socket2.async_receive_from (boost::asio::buffer (bytes2.data (), bytes2.size ()), endpoint4, [](boost::system::error_code const & error, size_t size_a) {
		ASSERT_FALSE (!error);
		ASSERT_EQ (16, size_a);
	});
	socket1.async_send_to (boost::asio::buffer (std::array<uint8_t, 16>{}, 16), endpoint6, [](boost::system::error_code const & error, size_t size_a) {
		ASSERT_FALSE (error);
		ASSERT_EQ (16, size_a);
	});
}

TEST (network, endpoint_bad_fd)
{
	germ::system system (24000, 1);
	system.nodes[0]->stop ();
	auto endpoint (system.nodes[0]->network.endpoint ());
	ASSERT_TRUE (endpoint.address ().is_loopback ());
	ASSERT_EQ (0, endpoint.port ());
}

TEST (network, reserved_address)
{
	ASSERT_FALSE (germ::reserved_address (germ::endpoint (boost::asio::ip::address_v6::from_string ("2001::"), 0), true));
	germ::endpoint loopback (boost::asio::ip::address_v6::from_string ("::1"), 1);
	ASSERT_FALSE (germ::reserved_address (loopback, false));
	ASSERT_TRUE (germ::reserved_address (loopback, true));
}

TEST (node, port_mapping)
{
	germ::system system (24000, 1);
	auto node0 (system.nodes[0]);
	node0->port_mapping.refresh_devices ();
	node0->port_mapping.start ();
	auto end (std::chrono::steady_clock::now () + std::chrono::seconds (500));
	//while (std::chrono::steady_clock::now () < end)
	{
		system.poll ();
	}
}
