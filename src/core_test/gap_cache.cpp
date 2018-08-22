#include <gtest/gtest.h>
#include <src/node/testing.hpp>

TEST (gap_cache, add_new)
{
	germ::system system (24000, 1);
	germ::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<germ::send_block> (0, 1, 2, germ::keypair ().prv, 4, 5));
	germ::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	cache.add (transaction, block1);
}

TEST (gap_cache, add_existing)
{
	germ::system system (24000, 1);
	germ::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<germ::send_block> (0, 1, 2, germ::keypair ().prv, 4, 5));
	germ::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	cache.add (transaction, block1);
	auto existing1 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing1);
	auto arrival (existing1->arrival);
	while (arrival == std::chrono::steady_clock::now ())
		;
	cache.add (transaction, block1);
	ASSERT_EQ (1, cache.blocks.size ());
	auto existing2 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing2);
	ASSERT_GT (existing2->arrival, arrival);
}

TEST (gap_cache, comparison)
{
	germ::system system (24000, 1);
	germ::gap_cache cache (*system.nodes[0]);
	auto block1 (std::make_shared<germ::send_block> (1, 0, 2, germ::keypair ().prv, 4, 5));
	germ::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
	cache.add (transaction, block1);
	auto existing1 (cache.blocks.get<1> ().find (block1->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing1);
	auto arrival (existing1->arrival);
	while (std::chrono::steady_clock::now () == arrival)
		;
	auto block3 (std::make_shared<germ::send_block> (0, 42, 1, germ::keypair ().prv, 3, 4));
	cache.add (transaction, block3);
	ASSERT_EQ (2, cache.blocks.size ());
	auto existing2 (cache.blocks.get<1> ().find (block3->hash ()));
	ASSERT_NE (cache.blocks.get<1> ().end (), existing2);
	ASSERT_GT (existing2->arrival, arrival);
	ASSERT_EQ (arrival, cache.blocks.get<1> ().begin ()->arrival);
}

TEST (gap_cache, gap_bootstrap)
{
	germ::system system (24000, 2);
	germ::block_hash latest (system.nodes[0]->latest (germ::test_genesis_key.pub));
	germ::keypair key;
	auto send (std::make_shared<germ::send_block> (latest, key.pub, germ::genesis_amount - 100, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (latest)));
	{
		germ::transaction transaction (system.nodes[0]->store.environment, nullptr, true);
		ASSERT_EQ (germ::process_result::progress, system.nodes[0]->block_processor.process_receive_one (transaction, send).code);
	}
	ASSERT_EQ (germ::genesis_amount - 100, system.nodes[0]->balance (germ::genesis_account));
	ASSERT_EQ (germ::genesis_amount, system.nodes[1]->balance (germ::genesis_account));
	system.wallet (0)->insert_adhoc (germ::test_genesis_key.prv);
	system.wallet (0)->insert_adhoc (key.prv);
	system.wallet (0)->send_action (germ::test_genesis_key.pub, key.pub, 100);
	ASSERT_EQ (germ::genesis_amount - 200, system.nodes[0]->balance (germ::genesis_account));
	ASSERT_EQ (germ::genesis_amount, system.nodes[1]->balance (germ::genesis_account));
	auto iterations2 (0);
	while (system.nodes[1]->balance (germ::genesis_account) != germ::genesis_amount - 200)
	{
		system.poll ();
		++iterations2;
		ASSERT_LT (iterations2, 200);
	}
}

TEST (gap_cache, two_dependencies)
{
	germ::system system (24000, 1);
	germ::keypair key;
	germ::genesis genesis;
	auto send1 (std::make_shared<germ::send_block> (genesis.hash (), key.pub, 1, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (genesis.hash ())));
	auto send2 (std::make_shared<germ::send_block> (send1->hash (), key.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, system.work.generate (send1->hash ())));
	auto open (std::make_shared<germ::open_block> (send1->hash (), key.pub, key.pub, key.prv, key.pub, system.work.generate (key.pub)));
	ASSERT_EQ (0, system.nodes[0]->gap_cache.blocks.size ());
	system.nodes[0]->block_processor.add (send2, std::chrono::steady_clock::now ());
	system.nodes[0]->block_processor.flush ();
	ASSERT_EQ (1, system.nodes[0]->gap_cache.blocks.size ());
	system.nodes[0]->block_processor.add (open, std::chrono::steady_clock::now ());
	system.nodes[0]->block_processor.flush ();
	ASSERT_EQ (2, system.nodes[0]->gap_cache.blocks.size ());
	system.nodes[0]->block_processor.add (send1, std::chrono::steady_clock::now ());
	system.nodes[0]->block_processor.flush ();
	ASSERT_EQ (0, system.nodes[0]->gap_cache.blocks.size ());
	germ::transaction transaction (system.nodes[0]->store.environment, nullptr, false);
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, send1->hash ()));
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, send2->hash ()));
	ASSERT_TRUE (system.nodes[0]->store.block_exists (transaction, open->hash ()));
}
