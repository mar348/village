#include <gtest/gtest.h>

#include <src/node/common.hpp>

TEST (message, keepalive_serialization)
{
	germ::keepalive request1;
	std::vector<uint8_t> bytes;
	{
		germ::vectorstream stream (bytes);
		request1.serialize (stream);
	}
	auto error (false);
	germ::bufferstream stream (bytes.data (), bytes.size ());
	germ::message_header header (error, stream);
	ASSERT_FALSE (error);
	germ::keepalive request2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (request1, request2);
}

TEST (message, keepalive_deserialize)
{
	germ::keepalive message1;
	message1.peers[0] = germ::endpoint (boost::asio::ip::address_v6::loopback (), 10000);
	std::vector<uint8_t> bytes;
	{
		germ::vectorstream stream (bytes);
		message1.serialize (stream);
	}
	germ::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	germ::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (germ::message_type::keepalive, header.type);
	germ::keepalive message2 (error, stream, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (message1.peers, message2.peers);
}

TEST (message, publish_serialization)
{
	germ::publish publish (std::unique_ptr<germ::tx> (new germ::send_block (0, 1, 2, germ::keypair ().prv, 4, 5)));
	ASSERT_EQ (germ::block_type::send, publish.header.block_type ());
	ASSERT_FALSE (publish.header.ipv4_only ());
	publish.header.ipv4_only_set (true);
	ASSERT_TRUE (publish.header.ipv4_only ());
	std::vector<uint8_t> bytes;
	{
		germ::vectorstream stream (bytes);
		publish.header.serialize (stream);
	}
	ASSERT_EQ (8, bytes.size ());
	ASSERT_EQ (0x52, bytes[0]);
	ASSERT_EQ (0x41, bytes[1]);
	ASSERT_EQ (germ::protocol_version, bytes[2]);
	ASSERT_EQ (germ::protocol_version, bytes[3]);
	ASSERT_EQ (germ::protocol_version_min, bytes[4]);
	ASSERT_EQ (static_cast<uint8_t> (germ::message_type::publish), bytes[5]);
	ASSERT_EQ (0x02, bytes[6]);
	ASSERT_EQ (static_cast<uint8_t> (germ::block_type::send), bytes[7]);
	germ::bufferstream stream (bytes.data (), bytes.size ());
	auto error (false);
	germ::message_header header (error, stream);
	ASSERT_FALSE (error);
	ASSERT_EQ (germ::protocol_version_min, header.version_min);
	ASSERT_EQ (germ::protocol_version, header.version_using);
	ASSERT_EQ (germ::protocol_version, header.version_max);
	ASSERT_EQ (germ::message_type::publish, header.type);
}

TEST (message, confirm_ack_serialization)
{
	germ::keypair key1;
	auto vote (std::make_shared<germ::vote> (key1.pub, key1.prv, 0, std::unique_ptr<germ::tx> (new germ::send_block (0, 1, 2, key1.prv, 4, 5))));
	germ::confirm_ack con1 (vote);
	std::vector<uint8_t> bytes;
	{
		germ::vectorstream stream1 (bytes);
		con1.serialize (stream1);
	}
	germ::bufferstream stream2 (bytes.data (), bytes.size ());
	bool error;
	germ::message_header header (error, stream2);
	germ::confirm_ack con2 (error, stream2, header);
	ASSERT_FALSE (error);
	ASSERT_EQ (con1, con2);
}
