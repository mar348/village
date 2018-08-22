#include <src/lib/interface.h>
#include <src/node/utility.hpp>
#include <src/node/working.hpp>

#include <lmdb/libraries/liblmdb/lmdb.h>

#include <ed25519-donna/ed25519.h>

static std::vector<boost::filesystem::path> all_unique_paths;

boost::filesystem::path germ::working_path ()
{
    auto result (germ::app_path ());
    switch (germ::rai_network)
    {
        case germ::germ_networks::germ_test_network:
            result /= "GermBlocksTest";
            break;
        case germ::germ_networks::germ_beta_network:
            result /= "GermBlocksBeta";
            break;
        case germ::germ_networks::germ_live_network:
            result /= "GermBlocks";
            break;
    }
    return result;
}

boost::filesystem::path germ::unique_path ()
{
    auto result (working_path () / boost::filesystem::unique_path ());
    all_unique_paths.push_back (result);
    return result;
}

std::vector<boost::filesystem::path> germ::remove_temporary_directories ()
{
    for (auto & path : all_unique_paths)
    {
        boost::system::error_code ec;
        boost::filesystem::remove_all (path, ec);
        if (ec)
        {
            std::cerr << "Could not remove temporary directory: " << ec.message () << std::endl;
        }

        // lmdb creates a -lock suffixed file for its MDB_NOSUBDIR databases
        auto lockfile = path;
        lockfile += "-lock";
        boost::filesystem::remove (lockfile, ec);
        if (ec)
        {
            std::cerr << "Could not remove temporary lock file: " << ec.message () << std::endl;
        }
    }
    return all_unique_paths;
}

germ::mdb_env::mdb_env (bool & error_a, boost::filesystem::path const & path_a, int max_dbs)
{
    boost::system::error_code error;
    if (!path_a.has_parent_path ())
    {
        error_a = true;
        environment = nullptr;
        return ;
    }

    boost::filesystem::create_directories (path_a.parent_path (), error);
    if (error)
    {
        error_a = true;
        environment = nullptr;
        return ;
    }

    auto status1 (mdb_env_create (&environment));
    assert (status1 == 0);
    auto status2 (mdb_env_set_maxdbs (environment, max_dbs));
    assert (status2 == 0);
    auto status3 (mdb_env_set_mapsize (environment, 1ULL * 1024 * 1024 * 1024 * 128)); // 128 Gigabyte
    assert (status3 == 0);
    // It seems if there's ever more threads than mdb_env_set_maxreaders has read slots available, we get failures on transaction creation unless MDB_NOTLS is specified
    // This can happen if something like 256 io_threads are specified in the node config
    auto status4 (mdb_env_open (environment, path_a.string ().c_str (), MDB_NOSUBDIR | MDB_NOTLS, 00600));
    error_a = status4 != 0;
}

germ::mdb_env::~mdb_env ()
{
    if (environment != nullptr)
    {
        mdb_env_close (environment);
    }
}

germ::mdb_env::operator MDB_env * () const
{
    return environment;
}

germ::mdb_val::mdb_val () :
value ({ 0, nullptr })
{
}

germ::mdb_val::mdb_val (MDB_val const & value_a) :
value (value_a)
{
}

germ::mdb_val::mdb_val (size_t size_a, void * data_a) :
value ({ size_a, data_a })
{
}

germ::mdb_val::mdb_val (germ::uint128_union const & val_a) :
mdb_val (sizeof (val_a), const_cast<germ::uint128_union *> (&val_a))
{
}

germ::mdb_val::mdb_val (germ::uint256_union const & val_a) :
mdb_val (sizeof (val_a), const_cast<germ::uint256_union *> (&val_a))
{
}

void * germ::mdb_val::data () const
{
    return value.mv_data;
}

size_t germ::mdb_val::size () const
{
    return value.mv_size;
}

germ::uint256_union germ::mdb_val::uint256 () const
{
    germ::uint256_union result;
    assert (size () == sizeof (result));
    std::copy (reinterpret_cast<uint8_t const *> (data ()), reinterpret_cast<uint8_t const *> (data ()) + sizeof (result), result.bytes.data ());
    return result;
}

germ::mdb_val::operator MDB_val * () const
{
    // Allow passing a temporary to a non-c++ function which doesn't have constness
    return const_cast<MDB_val *> (&value);
};

germ::mdb_val::operator MDB_val const & () const
{
    return value;
}

germ::transaction::transaction (germ::mdb_env & environment_a, MDB_txn * parent_a, bool write) :
environment (environment_a)
{
    auto status (mdb_txn_begin (environment_a, parent_a, write ? 0 : MDB_RDONLY, &handle));
    assert (status == 0);
}

germ::transaction::~transaction ()
{
    auto status (mdb_txn_commit (handle));
    assert (status == 0);
}

germ::transaction::operator MDB_txn * () const
{
    return handle;
}

void germ::open_or_create (std::fstream & stream_a, std::string const & path_a)
{
    stream_a.open (path_a, std::ios_base::in);
    if (stream_a.fail ())
    {
        stream_a.open (path_a, std::ios_base::out);
    }
    stream_a.close ();
    stream_a.open (path_a, std::ios_base::in | std::ios_base::out);
}
