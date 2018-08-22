//
// Created by daoful on 18-7-23.
//

#include <src/lib/smart_contract.h>


germ::smart_contract::smart_contract(germ::account const & address, std::string const & bytecode)
:contract_address(address),
 contract_bytecode(bytecode)
{
}

germ::smart_contract::smart_contract(bool & error, germ::stream & stream_r)
{
    if (error)
        return ;

    error = germ::read(stream_r, contract_address.bytes);
    if (error)
        return ;

    int len;
    error = germ::read(stream_r, len);
    if (error)
        return ;

    const int len_code = len;
//    char codes[len_code];
//    std::array<char, len_code> codes;
//    error = germ::read(stream_r, codes);
//    if (error)
//        return ;

//    contract_bytecode = std::string(codes);
}

germ::smart_contract::smart_contract(bool & error, boost::property_tree::ptree const & tree_r)
{
    try
    {
        if (error)
            return ;

        auto sc_info(tree_r.get_child("contract_info"));
        auto sc_info_address(sc_info.get<std::string>("address"));
        auto sc_info_bytes(sc_info.get<std::string>("bytecode"));

        error = contract_address.decode_account(sc_info_address);
        if (error)
            return ;

        contract_bytecode = sc_info_bytes;
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }
}

void germ::smart_contract::serialize(germ::stream &stream_r) const
{
    write (stream_r, contract_address.bytes);

    write (stream_r, contract_bytecode.length());

//    const int length = contract_bytecode.length();
//    char codes[length];
//    for (int j = 0; j < length; ++j) {
//        codes[j] = contract_bytecode[j];
//    }
//    write (stream_r, codes);
}

void germ::smart_contract::serialize_json(std::string & string_r) const
{
    boost::property_tree::ptree tree;

    boost::property_tree::ptree tree_smart_contract;
    std::string address;
    contract_address.encode_account(address);
    tree_smart_contract.put("address", address);
    tree_smart_contract.put("bytecode", contract_bytecode);
    tree.add_child("contract_info", tree_smart_contract);

    std::stringstream ostream;
    boost::property_tree::write_json(ostream, tree);
    string_r = ostream.str();
}

bool germ::smart_contract::deserialize(germ::stream &stream_r)
{
    auto error(false);

    error = germ::read(stream_r, contract_address.bytes);
    if (error)
        return error;

    int len;
    error = germ::read(stream_r, len);
    if (error)
        return error;

//    const int len_code = len;
//    char codes[len_code];
//    error = germ::read(stream_r, codes);
//    if (error)
//        return error;
//
//    contract_bytecode = std::string(codes);

    return error;
}

bool germ::smart_contract::deserialize_json(boost::property_tree::ptree const & tree_r)
{
    auto error(false);
    try
    {
        auto sc_info(tree_r.get_child("contract_info"));
        auto sc_info_address(sc_info.get<std::string>("address"));
        auto sc_info_bytes(sc_info.get<std::string>("bytecode"));

        error = contract_address.decode_account(sc_info_address);
        if (error)
            return error;

        contract_bytecode = sc_info_bytes;
    }
    catch (std::runtime_error const &)
    {
        error = true;
    }

    return error;
}

bool germ::smart_contract::operator==(germ::smart_contract const &other_r) const
{
    auto result(contract_address == other_r.contract_address && contract_bytecode == other_r.contract_bytecode);
    return result;
}

germ::contract_type germ::smart_contract::type() const
{
    return germ::contract_type::regular;
}

bool germ::smart_contract::valid_predecessor(germ::smart_contract const & smart_contract) const
{
    bool result;
    switch (smart_contract.type())
    {
        case germ::contract_type ::election:
        case germ::contract_type ::epoch:
        case germ::contract_type ::witness:
        case germ::contract_type ::regular:
            result = true;
            break;
        default:
            result = false;
            break;
    }
    return result;
}

