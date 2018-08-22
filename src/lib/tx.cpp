//
// Created by daoful on 18-7-23.
//

#include <src/lib/tx.h>

germ::tx_message::tx_message():
value(0),
data(""),
gas(0),
gas_price(0)
{
}

germ::tx_message::tx_message(uint64_t const &tx_value,
        std::string const &tx_data, uint64_t const &tx_gas,
        uint64_t const &tx_gas_price)
:value(tx_value),
data(tx_data),
gas(tx_gas),
gas_price(tx_gas_price)
{
}

germ::tx_message::tx_message(germ::tx_message const &tx_info):
value(tx_info.value),
data(tx_info.data),
gas(tx_info.gas),
gas_price(tx_info.gas_price)
{

}

std::string germ::tx::to_json()
{
    std::string result;
    serialize_json(result);
    
    return result;
}

germ::block_hash germ::tx::hash() const
{
    germ::uint256_union result;
    blake2b_state hash_;
    auto status (blake2b_init(&hash_, sizeof(result.bytes)));
    assert(status == 0);
    hash(hash_);
    status = blake2b_final(&hash_, result.bytes.data(), sizeof(result.bytes));
    assert(status == 0);

    return result;
}

germ::tx::tx(germ::block_hash const &previous_r, germ::account const &destination_r, germ::block_hash source_r, germ::account const & account, germ::amount const & balance_r,
             germ::tx_message const &tx_info_r, germ::epoch_hash const &epoch_r, germ::raw_key const &prv_r,
             germ::public_key const &pub_r):
previous_(previous_r),
destination_(destination_r),
source_(source_r),
balance_(balance_r),
account_(account),
tx_info(tx_info_r),
epoch(epoch_r),
signature(germ::sign_message(prv_r, pub_r, hash()))
{

}

germ::tx::tx(bool &error, germ::stream &stream)
{
    if (error)
        return ;

    if ((error = germ::read(stream, previous_.bytes)))
        return ;

    if ((error = germ::read(stream, destination_.bytes)))
        return ;

    if ((error = germ::read(stream, source_.bytes)))
        return ;

    if ((error = germ::read(stream, balance_.bytes)))
        return ;

    if ((error = germ::read(stream, account_.bytes)))
        return ;

    if ((error = germ::read(stream, tx_info.value)))
        return ;

    size_t data_len;
    if ((error = germ::read(stream, data_len)))
        return ;

    if (data_len != 0)
    {
        std::vector<uint8_t> datas(data_len);
        if ((error = germ::read_data(stream, datas)))
            return;

        tx_info.data = germ::to_string(datas);
    }
    else
    {
        tx_info.data = "";
    }

    if ((error = germ::read(stream, tx_info.gas)))
        return ;

    if ((error = germ::read(stream, tx_info.gas_price)))
        return ;

    if ((error = germ::read(stream, epoch.bytes)))
        return ;

    error = germ::read(stream, signature.bytes);
}

germ::tx::tx(bool &error, boost::property_tree::ptree const &tree)
{
    try
    {
        if (error)
            return ;

        auto previous(tree.get<std::string>("previous"));
        auto destination(tree.get<std::string>("destination"));
        auto source(tree.get<std::string>("source"));
        auto balance(tree.get<std::string>("balance"));
        auto account(tree.get<std::string>("account"));

        auto tx(tree.get_child("tx_info"));
        auto tx_value(tx.get<std::string>("value"));
        auto tx_data(tx.get<std::string>("data"));
        auto tx_gas(tx.get<std::string>("gas"));
        auto tx_gasprice(tx.get<std::string>("gasprice"));

        auto epoch_r(tree.get<std::string>("epoch"));
        auto signature_r(tree.get<std::string>("signature"));

        if (!previous.empty() && (error = previous_.decode_hex(previous)))
            return ;

        if (!destination.empty() && (error = destination_.decode_account(destination)))
            return ;

        if (!source.empty() && (error = source_.decode_hex(source)))
            return ;

        if (!balance.empty() && (error = balance_.decode_hex(balance)))
            return ;

        if (!account.empty() && (error = account_.decode_account(account)))
            return ;

        if (!tx_value.empty() && (error = germ::from_string_hex(tx_value, tx_info.value)))
            return ;

        tx_info.data = tx_data;

        if (!tx_gas.empty() && (error = germ::from_string_hex(tx_gas, tx_info.gas)))
            return ;

        if (!tx_gasprice.empty() && (error = germ::from_string_hex(tx_gasprice, tx_info.gas_price)))
            return ;

        if (!epoch_r.empty() && (error = epoch.decode_hex(epoch_r)))
            return ;

        if (signature_r.empty() && (error = signature.decode_hex(signature_r)))
            return ;
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
}

germ::tx::~tx()
{
}

void hash_tx(blake2b_state &hash_r, germ::tx_message const & tx_info)
{
    auto status (blake2b_update (&hash_r, &tx_info.value, sizeof (tx_info.value)));
    assert (status == 0);

    status = blake2b_update (&hash_r, tx_info.data.data(), tx_info.data.length());
    assert (status == 0);

    status = blake2b_update (&hash_r, &tx_info.gas, sizeof (tx_info.gas));
    assert (status == 0);

    status = blake2b_update (&hash_r, &tx_info.gas_price, sizeof (tx_info.gas_price));
    assert (status == 0);
}

void germ::tx::hash(blake2b_state &hash_r) const
{
    auto status (blake2b_update (&hash_r, previous_.bytes.data (), sizeof (previous_.bytes)));
    assert (status == 0);

    status = blake2b_update (&hash_r, destination_.bytes.data (), sizeof (destination_.bytes));
    assert (status == 0);

    status = blake2b_update (&hash_r, source_.bytes.data (), sizeof (source_.bytes));
    assert (status == 0);

    status = blake2b_update(&hash_r, balance_.bytes.data(), sizeof (balance_.bytes));
    assert(status == 0);

    status = blake2b_update(&hash_r, account_.bytes.data(), sizeof (account_.bytes));
    assert(status == 0);

    hash_tx(hash_r, tx_info);

    status = blake2b_update (&hash_r, epoch.bytes.data (), sizeof (epoch.bytes));
    assert (status == 0);
}


germ::account germ::tx::destination() const
{
    return destination_;
}

germ::block_hash germ::tx::previous() const
{
    return previous_;
}

germ::block_hash germ::tx::source() const
{
    return source_;
}

germ::block_hash germ::tx::root() const
{
    return previous_;
}

void germ::tx::serialize(germ::stream & stream_r) const
{
    write(stream_r, previous_.bytes);
    write(stream_r, destination_.bytes);
    write(stream_r, source_.bytes);
    write(stream_r, balance_.bytes);
    write(stream_r, account_.bytes);
    write(stream_r, tx_info.value);

    size_t len = (size_t)tx_info.data.length();
    write(stream_r, len);

    if (len != 0)
    {
        std::vector<uint8_t> datas;
        germ::to_vector(datas, tx_info.data);
        germ::write_data(stream_r, datas);
    }

    write(stream_r, tx_info.gas);
    write(stream_r, tx_info.gas_price);
    write(stream_r, epoch.bytes);
    write(stream_r, signature.bytes);
}

void germ::tx::serialize_json(std::string &string_r) const
{
    boost::property_tree::ptree tree;
    germ::block_type tx_type = type();
    if (tx_type == germ::block_type::send)
    {
        tree.put("type", "send");
    }
    else if (tx_type == germ::block_type::receive)
    {
        tree.put("type", "receive");
    }

    std::string previous;
    previous_.encode_hex(previous);
    tree.put("previous", previous);

    std::string destination;
    destination_.encode_account(destination);
    tree.put("destination", destination);

    std::string source;
    source_.encode_hex(source);
    tree.put("source", source);

    std::string balance;
    balance_.encode_hex(balance);
    tree.put("balance", balance_.to_string_dec());

    std::string account;
    account_.encode_account(account);
    tree.put("account", account);

    boost::property_tree::ptree tx;
    tx.put("value", germ::to_string_hex(tx_info.value));
    tx.put("data", tx_info.data);

    tx.put("gas", germ::to_string_hex(tx_info.gas));
    tx.put("gasprice", germ::to_string_hex(tx_info.gas_price));

    tree.put_child("tx_info", tx);

    std::string signature_r;
    signature.encode_hex(signature_r);
    tree.put("signature", signature_r);

    std::stringstream ostream;
    boost::property_tree::write_json(ostream, tree);
    string_r = ostream.str();
}

bool germ::tx::deserialize(germ::stream &stream_r)
{
    auto error(false);

    if ((error = germ::read(stream_r, destination_.bytes)))
        return error;

    if ((error = germ::read(stream_r, source_.bytes)))
        return error;

    if ((error = germ::read(stream_r, balance_.bytes)))
        return error;

    if ((error = germ::read(stream_r, account_.bytes)))
        return error;

    if ((error = germ::read(stream_r, tx_info.value)))
        return error;

    size_t data_len;
    if ((error = germ::read(stream_r, data_len)))
        return error;

    std::vector<uint8_t> datas(data_len);
    if ((error = germ::read_data(stream_r, datas)))
        return error;

    if (data_len != 0)
    {
        std::vector<uint8_t> datas(data_len);
        if ((error = germ::read_data(stream_r, datas)))
            return error;

        tx_info.data = germ::to_string(datas);
    }
    else
    {
        tx_info.data = "";
    }

    if((error = germ::read(stream_r, tx_info.gas)))
        return error;

    if ((error = germ::read(stream_r, tx_info.gas_price)))
        return error;

    if ((error = germ::read(stream_r, epoch.bytes)))
        return error;

    error = germ::read(stream_r, signature.bytes);

    return error;
}

bool germ::tx::deserialize_json(boost::property_tree::ptree const &tree_r)
{
    auto error(false);
    try
    {
        auto tx_type(tree_r.get<std::string>("type"));
        assert(tx_type == "send" || tx_type == "receive");
        auto previous(tree_r.get<std::string>("previous"));
        auto destination(tree_r.get<std::string>("destination"));
        auto source(tree_r.get<std::string>("source"));
        auto balance(tree_r.get<std::string>("balance"));
        auto account(tree_r.get<std::string>("account"));

        auto tx(tree_r.get_child("tx_info"));
        auto tx_value(tx.get<std::string>("value"));
        auto tx_data(tx.get<std::string>("data"));
        auto tx_gas(tx.get<std::string>("gas"));
        auto tx_gasprice(tx.get<std::string>("gasprice"));

        auto epoch_r(tree_r.get<std::string>("epoch"));
        auto signature_r(tree_r.get<std::string>("signature"));

        if (!previous.empty() && (error = previous_.decode_hex(previous)))
            return error;

        if (!destination.empty() && (error = destination_.decode_account(destination)))
            return error;

        if (!source.empty() && (error = source_.decode_hex(source)))
            return error;

        if (!balance.empty() && (error = balance_.decode_hex(balance)))
            return error;

        if (!account.empty() && (error = account_.decode_account(account)))
            return error;

        if (!tx_value.empty() && (error = germ::from_string_hex(tx_value, tx_info.value)))
            return error;

        tx_info.data = tx_data;

        if (!tx_gas.empty() && (error = germ::from_string_hex(tx_gas, tx_info.gas)))
            return error;

        if (!tx_gasprice.empty() && (error = germ::from_string_hex(tx_gasprice, tx_info.gas_price)))
            return error;

        if (!epoch_r.empty() && (error = epoch.decode_hex(epoch_r)))
            return error;

        if (signature_r.empty() && (error = signature.decode_hex(signature_r)))
            return error;
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }

    return error;
}

bool germ::tx::operator==(germ::tx const &other) const
{
    auto result(previous_ == other.previous_ && destination_ == other.destination_ && source_ == other.source_ && balance_ == other.balance_ && account_ == other.account_ &&
                tx_info.value == other.tx_info.value && tx_info.data == other.tx_info.data && tx_info.gas == other.tx_info.gas && tx_info.gas_price == other.tx_info.gas_price &&
                epoch == other.epoch && signature == other.signature
            );

    return result;
}

void germ::tx::visit(germ::block_visitor &visit_r) const
{
    visit_r.tx(*this);
}

germ::block_type germ::tx::type() const
{
    germ::block_type tx_type(germ::block_type::not_a_block);
    if (!destination_.is_zero())
    {
//        if (!tx_info.data.empty())
//        {
//            tx_type = germ::block_type::vote;
//        }
//        else
        {
            tx_type = germ::block_type::send;
        }
    }
    else if (!source_.is_zero())
    {
        tx_type = germ::block_type::receive;
    }
    else
    {
        tx_type = germ::block_type::not_a_block;
    }

    return tx_type;
}

germ::signature germ::tx::block_signature() const
{
    return signature;
}

void germ::tx::signature_set(germ::uint512_union const &signature_r)
{
    signature = signature_r;
}

bool germ::tx::valid_predecessor(germ::tx const &tx) const
{
    auto result(false);
    germ::block_type tx_type = type();
    switch (tx_type)
    {
        case germ::block_type::send:
        case germ::block_type::receive:
        case germ::block_type::vote:
            result = true;
            break;
        default:
            result = false;
            break;
    }

    return result;
}

size_t germ::tx::size() const
{
    size_t size;
    size = sizeof (previous_) + sizeof (destination_) + sizeof (source_) + sizeof (balance_) + sizeof(account_) +
           sizeof (tx_info.value) + tx_info.data.length() + sizeof (tx_info.gas) + sizeof (tx_info.gas_price) +
           sizeof (epoch) +  sizeof(size_t) + 
           sizeof (signature);

    return size;
}













