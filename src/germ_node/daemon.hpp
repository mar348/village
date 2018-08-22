#include <src/node/node.hpp>
#include <src/node/rpc.hpp>

namespace rai_daemon
{
class daemon
{
public:
    void run (boost::filesystem::path const &);
};
class daemon_config
{
public:
    daemon_config (boost::filesystem::path const &);
    bool deserialize_json (bool &, boost::property_tree::ptree &);
    void serialize_json (boost::property_tree::ptree &);
    bool upgrade_json (unsigned, boost::property_tree::ptree &);
    bool rpc_enable;
    germ::rpc_config rpc;
    germ::node_config node;
    bool opencl_enable;
    germ::opencl_config opencl;
};
}
