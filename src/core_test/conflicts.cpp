#include <gtest/gtest.h>
#include <src/node/testing.hpp>

TEST (conflicts, start_stop)
{
	germ::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	germ::genesis genesis;
	germ::keypair key1;
	auto send1 (std::make_shared<germ::send_block> (genesis.hash (), key1.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	ASSERT_EQ (germ::process_result::progress, node1.process (*send1).code);
	ASSERT_EQ (0, node1.active.roots.size ());
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto root1 (send1->root ());
	auto existing1 (node1.active.roots.find (root1));
	ASSERT_NE (node1.active.roots.end (), existing1);
	auto votes1 (existing1->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (1, votes1->votes.rep_votes.size ());
}

TEST (conflicts, add_existing)
{
	germ::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	germ::genesis genesis;
	germ::keypair key1;
	auto send1 (std::make_shared<germ::send_block> (genesis.hash (), key1.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	ASSERT_EQ (germ::process_result::progress, node1.process (*send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	germ::keypair key2;
	auto send2 (std::make_shared<germ::send_block> (genesis.hash (), key2.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	node1.active.start (send2);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto vote1 (std::make_shared<germ::vote> (key2.pub, key2.prv, 0, send2));
	node1.active.vote (vote1);
	ASSERT_EQ (1, node1.active.roots.size ());
	auto votes1 (node1.active.roots.find (send2->root ())->election);
	ASSERT_NE (nullptr, votes1);
	ASSERT_EQ (2, votes1->votes.rep_votes.size ());
	ASSERT_NE (votes1->votes.rep_votes.end (), votes1->votes.rep_votes.find (key2.pub));
}

TEST (conflicts, add_two)
{
	germ::system system (24000, 1);
	auto & node1 (*system.nodes[0]);
	germ::genesis genesis;
	germ::keypair key1;
	auto send1 (std::make_shared<germ::send_block> (genesis.hash (), key1.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	ASSERT_EQ (germ::process_result::progress, node1.process (*send1).code);
	auto node_l (system.nodes[0]);
	node1.active.start (send1);
	germ::keypair key2;
	auto send2 (std::make_shared<germ::send_block> (send1->hash (), key2.pub, 0, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	ASSERT_EQ (germ::process_result::progress, node1.process (*send2).code);
	node1.active.start (send2);
	ASSERT_EQ (2, node1.active.roots.size ());
}

TEST (votes, contested)
{
	germ::genesis genesis;
	auto block1 (std::make_shared<germ::state_block> (germ::test_genesis_key.pub, genesis.hash (), germ::test_genesis_key.pub, germ::genesis_amount - germ::Gxrb_ratio, germ::test_genesis_key.pub, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	auto block2 (std::make_shared<germ::state_block> (germ::test_genesis_key.pub, genesis.hash (), germ::test_genesis_key.pub, germ::genesis_amount - 2 * germ::Gxrb_ratio, germ::test_genesis_key.pub, germ::test_genesis_key.prv, germ::test_genesis_key.pub, 0));
	ASSERT_FALSE (*block1 == *block2);
	germ::votes votes (block1);
	ASSERT_TRUE (votes.uncontested ());
	votes.rep_votes[germ::test_genesis_key.pub] = block2;
	ASSERT_FALSE (votes.uncontested ());
}
