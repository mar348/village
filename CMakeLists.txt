cmake_minimum_required (VERSION 2.8.11)
project (src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CPACK_PACKAGE_VERSION_MAJOR "15")
set (CPACK_PACKAGE_VERSION_MINOR "0")
set (CPACK_PACKAGE_VERSION_PATCH "0")
if (DEFINED GIT_COMMIT)
set (CPACK_PACKAGE_VERSION_PATCH "GIT-${GIT_COMMI11T}")
endif (DEFINED GIT_COMMIT)
set (CPACK_PACKAGE_VENDOR "Nano Currency")

set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")

set (GERMBLOCKS_GUI OFF CACHE BOOL "")
set (GERMBLOCKS_TEST OFF CACHE BOOL "")
set (GERMBLOCKS_SECURE_RPC OFF CACHE BOOL "")

option(GERMBLOCKS_ASAN_INT "Enable ASan+UBSan+Integer overflow" OFF)
option(GERMBLOCKS_ASAN "Enable ASan+UBSan" OFF)
option(GERMBLOCKS_SIMD_OPTIMIZATIONS "Enable CPU-specific SIMD optimizations (SSE/AVX or NEON, e.g.)" OFF)
set (BOOST_CUSTOM OFF CACHE BOOL "")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

if(WIN32)
   if(MSVC_VERSION)
       if(MSVC_VERSION GREATER_EQUAL 1910)
        add_definitions(-DGTEST_LANG_CXX11=1)
        add_definitions(-DGTEST_HAS_TR1_TUPLE=0)
       endif()
   endif()
endif()

if (WIN32)
    set (PLATFORM_COMPILE_FLAGS "-DBOOST_SPIRIT_THREADSAFE -D_WIN32_WINNT=0x0600 -DWINVER=0x0600 -DWIN32_LEAN_AND_MEAN -DMINIUPNP_STATICLIB")
    set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
else (WIN32)
    set (PLATFORM_COMPILE_FLAGS "-DBOOST_SPIRIT_THREADSAFE -Werror=switch -fPIC")
    if (GERMBLOCKS_ASAN)
        set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=address,undefined -DED25519_NO_INLINE_ASM")
    elseif (GERMBLOCKS_ASAN_INT)
        set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=address,undefined,integer -DED25519_NO_INLINE_ASM")
    elseif (GERMBLOCKS_TSAN)
        set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -fsanitize=thread -DED25519_NO_INLINE_ASM")
    endif()

    IF (CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86(_64)?)$")
        if (GERMBLOCKS_SIMD_OPTIMIZATIONS)
            set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -msse4")
        elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
            set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -msse2")
        endif()

        set (BLAKE2_IMPLEMENTATION "blake2/blake2b.c")
        if (ENABLE_AVX2)
            set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -mavx2 -mbmi -mbmi2")
            if (PERMUTE_WITH_GATHER)
                set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -DPERMUTE_WITH_GATHER")
            elseif (PERMUTE_WITH_SHUFFLES)
                set (PLATFORM_COMPILE_FLAGS "${PLATFORM_COMPILE_FLAGS} -DPERMUTE_WITH_SHUFFLES")
            endif()
        endif()
    else()
        set (BLAKE2_IMPLEMENTATION "blake2/blake2b-ref.c")
    endif()
endif (WIN32)

if (WIN32)
    set (PLATFORM_C_FLAGS "/std=c11")
else (WIN32)
    set (PLATFORM_C_FLAGS "-std=c11")
endif (WIN32)

if (WIN32)
    set (PLATFORM_CXX_FLAGS "/bigobj")
else (WIN32)
    set (PLATFORM_CXX_FLAGS "-std=c++14")
endif (WIN32)

if (WIN32)
    set (PLATFORM_GUI_TARGET_PROPERTIES WIN32)
else (WIN32)
    set (PLATFORM_GUI_TARGET_PROPERTIES "")
endif (WIN32)

if (APPLE)
    set (PLATFORM_LINK_FLAGS "-framework Foundation -framework OpenCL")
elseif (WIN32)
    set (PLATFORM_LINK_FLAGS "")
else ()
    set (PLATFORM_LINK_FLAGS "-static-libgcc -static-libstdc++")
    if (GERMBLOCKS_ASAN)
        set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined")
    elseif (GERMBLOCKS_ASAN_INT)
        set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=address,undefined,integer")
    elseif (GERMBLOCKS_TSAN)
        set (PLATFORM_LINK_FLAGS "${PLATFORM_LINK_FLAGS} -fsanitize=thread")
    endif()
endif (APPLE)

if (GERMBLOCKS_GUI)
    if (WIN32)
        set (PLATFORM_QT_PACKAGES WinExtras)
    else (WIN32)
        set (PLATFORM_QT_PACKAGES)
    endif (WIN32)

    find_package (Qt5 COMPONENTS Core Gui Widgets Test ${PLATFORM_QT_PACKAGES})
    include_directories (${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Test_INCLUDE_DIRS})
endif (GERMBLOCKS_GUI)

if (GERMBLOCKS_SECURE_RPC)
    find_package (OpenSSL 1.0 EXACT REQUIRED)
    include_directories(${OPENSSL_INCLUDE_DIR})
    add_definitions (-DGERMBLOCKS_SECURE_RPC)
    message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
    message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")
    message("OpenSSL lib: ${OPENSSL_SSL_LIBRARY}")
    message("Crypto lib: ${OPENSSL_CRYPTO_LIBRARY}")
else ()
    set (OPENSSL_LIBRARIES "")
endif (GERMBLOCKS_SECURE_RPC)

include_directories (${CMAKE_SOURCE_DIR})

set(Boost_USE_STATIC_LIBS        ON)
set(Boost_USE_MULTITHREADED      ON)

if (BOOST_CUSTOM)
    list(APPEND CMAKE_MODULE_PATH ".\\" ${CMAKE_SOURCE_PATH})
endif (BOOST_CUSTOM)

find_package (Boost 1.66.0 REQUIRED COMPONENTS date_time filesystem system log log_setup thread program_options regex chrono atomic)
include_directories (${Boost_INCLUDE_DIR})

add_library (ed25519
    ed25519-donna/ed25519-donna-portable.h
    ed25519-donna/ed25519-hash-custom.h
    ed25519-donna/ed25519-randombytes-custom.h
    ed25519-donna/ed25519.h
    ed25519-donna/ed25519.c)

include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/include")
include_directories("${CMAKE_SOURCE_DIR}/phc-winner-argon2/src")
include_directories("${CMAKE_SOURCE_DIR}/blake2")

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set (ARGON_CORE phc-winner-argon2/src/opt.c)
else ()
    set (ARGON_CORE phc-winner-argon2/src/ref.c)
endif ()

if (WIN32)
    set (gtest_force_shared_crt ON)
else ()
    set (gtest_force_shared_crt OFF)
endif()
add_subdirectory (gtest)
include_directories ("gtest/include")

include_directories (beast/include)

set (UPNPC_BUILD_SHARED OFF CACHE BOOL "")
add_subdirectory (miniupnp/miniupnpc)
include_directories (miniupnp/miniupnpc)

set (BUILD_SHARED OFF CACHE BOOL "")
set (BUILD_TESTING OFF CACHE BOOL "")
set (USE_INTERMEDIATE_OBJECTS_TARGET OFF CACHE BOOL "")
if (CRYPTOPP_CUSTOM OR NOT GERMBLOCKS_SIMD_OPTIMIZATIONS)
    set (CRYPTOPP_LIBRARY cryptopp)
    add_library (cryptopp
    cryptopp/algparam.cpp
    cryptopp/asn.cpp
    cryptopp/basecode.cpp
    cryptopp/cpu.cpp
    cryptopp/cryptlib.cpp
    cryptopp/default.cpp
    cryptopp/des.cpp
    cryptopp/dessp.cpp
    cryptopp/dll.cpp
    cryptopp/ec2n.cpp
    cryptopp/ecp.cpp
    cryptopp/filters.cpp
    cryptopp/fips140.cpp
    cryptopp/gcm.cpp
    cryptopp/gf2n.cpp
    cryptopp/gfpcrypt.cpp
    cryptopp/hex.cpp
    cryptopp/hmac.cpp
    cryptopp/hrtimer.cpp
    cryptopp/integer.cpp
    cryptopp/iterhash.cpp
    cryptopp/misc.cpp
    cryptopp/modes.cpp
    cryptopp/mqueue.cpp
    cryptopp/nbtheory.cpp
    cryptopp/oaep.cpp
    cryptopp/osrng.cpp
    cryptopp/pubkey.cpp
    cryptopp/queue.cpp
    cryptopp/randpool.cpp
    cryptopp/rdtables.cpp
    cryptopp/rijndael.cpp
    cryptopp/rng.cpp
    cryptopp/sha.cpp
    cryptopp/simple.cpp
    cryptopp/winpipes.cpp)
    set_target_properties (cryptopp PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS}")
else ()
    set (CRYPTOPP_LIBRARY cryptopp-static)
    add_subdirectory (cryptopp)
endif ()

add_library (argon2
    phc-winner-argon2/src/argon2.c
    phc-winner-argon2/include/argon2.h
    phc-winner-argon2/src/core.c
    phc-winner-argon2/src/thread.c
    phc-winner-argon2/src/encoding.c
    ${ARGON_CORE})

add_library (xxhash
    xxhash/xxhash.c
    xxhash/xxhash.h)

add_library (lmdb
    lmdb/libraries/liblmdb/lmdb.h
    lmdb/libraries/liblmdb/mdb.c
    lmdb/libraries/liblmdb/midl.c)

add_library (blake2
    blake2/blake2-config.h
    blake2/blake2-impl.h
    blake2/blake2.h
    ${BLAKE2_IMPLEMENTATION})

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set (PLATFORM_LIB_SOURCE src/plat/default/priority.cpp)
    set (PLATFORM_SECURE_SOURCE src/plat/osx/working.mm)
    set (PLATFORM_WALLET_SOURCE src/plat/default/icon.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set (PLATFORM_LIB_SOURCE src/plat/windows/priority.cpp)
    set (PLATFORM_SECURE_SOURCE src/plat/windows/working.cpp)
    set (PLATFORM_NODE_SOURCE src/plat/windows/openclapi.cpp)
    set (PLATFORM_WALLET_SOURCE src/plat/windows/icon.cpp RaiBlocks.rc)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set (PLATFORM_LIB_SOURCE src/plat/linux/priority.cpp)
    set (PLATFORM_SECURE_SOURCE src/plat/posix/working.cpp)
    set (PLATFORM_NODE_SOURCE src/plat/posix/openclapi.cpp)
    set (PLATFORM_WALLET_SOURCE src/plat/default/icon.cpp)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD")
    set (PLATFORM_LIB_SOURCE src/plat/default/priority.cpp)
    set (PLATFORM_SECURE_SOURCE src/plat/posix/working.cpp)
    set (PLATFORM_NODE_SOURCE src/plat/posix/openclapi.cpp)
    set (PLATFORM_WALLET_SOURCE src/plat/default/icon.cpp)
else ()
    error ("Unknown platform: ${CMAKE_SYSTEM_NAME}")
endif ()

# Embed bootstrap representative weights in executable
file (READ rep_weights.bin filedata HEX)
string (REGEX REPLACE "(..)" "0x\\1," filedata ${filedata})
file (WRITE ${CMAKE_BINARY_DIR}/bootstrap_weights.cpp "#include <cstddef>\n"
    "namespace germ {\n"
    "    unsigned char germ_bootstrap_weights[] = {${filedata} 0x00};\n"
    "    size_t germ_bootstrap_weights_size = sizeof(germ_bootstrap_weights) - 1;\n"
    "}\n")
    
if (GERMBLOCKS_SECURE_RPC)
    set (SECURE_RPC_SOURCE src/node/rpc_secure.cpp src/node/rpc_secure.hpp)
endif ()

add_library (secure
    ${PLATFORM_SECURE_SOURCE}
    ${CMAKE_BINARY_DIR}/bootstrap_weights.cpp
    src/config.hpp
    src/common.cpp
    src/common.hpp
    src/blockstore.cpp
    src/blockstore.hpp
    src/ledger.cpp
    src/ledger.hpp
    src/node/utility.cpp
    src/node/utility.hpp
    src/versioning.hpp
    src/versioning.cpp
    src/epochstore.cpp
    src/epochstore.h
    src/epoch_common.cpp
    src/epoch_common.h)

SET (GERM_LIB_SOURCES
    ${PLATFORM_LIB_SOURCE}
    src/lib/errors.hpp
    src/lib/errors.cpp
    src/lib/expected.hpp
    src/lib/blocks.cpp
    src/lib/blocks.hpp
    src/lib/interface.cpp
    src/lib/interface.h
    src/lib/numbers.cpp
    src/lib/numbers.hpp
    src/lib/utility.cpp
    src/lib/utility.hpp
    src/lib/work.hpp
    src/lib/work.cpp
    src/lib/smart_contract.h
    src/lib/smart_contract.cpp
    src/lib/tx.h
    src/lib/tx.cpp
    src/lib/epoch.h
    src/lib/epoch.cpp
    src/lib/witness.h
    src/lib/witness.cpp
    src/lib/election.h
    src/lib/election.cpp
    src/lib/vote.h
    src/lib/vote.cpp
    src/lib/smart_contract_state.h
    src/lib/smart_contract_state.cpp
    src/lib/witness_candidate.cpp
    src/lib/witness_candidate.h
    src/lib/general_account.cpp 
    src/lib/general_account.h)

add_library (germ_lib SHARED ${GERM_LIB_SOURCES} )
add_library (germ_lib_static STATIC ${GERM_LIB_SOURCES})
if (APPLE)
    target_link_libraries(germ_lib /usr/local/lib/libboost_system.a )
endif (APPLE)

add_library (node
    ${PLATFORM_NODE_SOURCE}
    ${SECURE_RPC_SOURCE}
    src/node/bootstrap.cpp
    src/node/bootstrap.hpp
    src/node/common.cpp
    src/node/common.hpp
    src/node/node.hpp
    src/node/node.cpp
    src/node/openclwork.cpp
    src/node/openclwork.hpp
    src/node/rpc.hpp
    src/node/rpc.cpp
    src/node/testing.hpp
    src/node/testing.cpp
    src/node/wallet.hpp
    src/node/wallet.cpp
    src/node/stats.hpp
    src/node/stats.cpp
    src/node/working.hpp
    src/node/xorshift.hpp
    src/node/network/network.cpp
    src/node/network/network.h
    src/node/bootstrap/socket.cpp
    src/node/bootstrap/socket.h
    src/node/bootstrap/bootstrap_client.cpp
    src/node/bootstrap/bootstrap_client.h
    src/node/bootstrap/bootstrap_attempt.cpp
    src/node/bootstrap/bootstrap_attempt.h
    src/node/bootstrap/frontier_req_client.cpp
    src/node/bootstrap/frontier_req_client.h
    src/node/bootstrap/bulk_pull_client.cpp
    src/node/bootstrap/bulk_pull_client.h 
    src/node/bootstrap/bulk_push_client.cpp
    src/node/bootstrap/bulk_push_client.h
    src/node/bootstrap/bootstrap_initiator.cpp
    src/node/bootstrap/bootstrap_initiator.h
    src/node/bootstrap/bootstrap_server.cpp
    src/node/bootstrap/bootstrap_server.h
    src/node/bootstrap/bulk_pull_server.cpp
    src/node/bootstrap/bulk_pull_server.h
    src/node/bootstrap/bulk_pull_blocks_server.cpp
    src/node/bootstrap/bulk_pull_blocks_server.h
    src/node/bootstrap/bulk_push_server.cpp
    src/node/bootstrap/bulk_push_server.h
    src/node/bootstrap/frontier_req_server.cpp
    src/node/bootstrap/frontier_req_server.h
    src/node/deposit.cpp
    src/node/deposit.h
    src/node/consensus/active_witnesses.cpp 
    src/node/consensus/active_witnesses.h
    src/node/bootstrap/epoch_req_server.cpp
    src/node/bootstrap/epoch_req_server.h
    src/node/bootstrap/epoch_req_client.cpp
    src/node/bootstrap/epoch_req_client.h
    src/node/bootstrap/epoch_bulk_pull_server.cpp
    src/node/bootstrap/epoch_bulk_pull_server.h
    src/node/bootstrap/epoch_bulk_push_server.cpp
    src/node/bootstrap/epoch_bulk_push_server.h
    src/node/bootstrap/epoch_bulk_pull_client.cpp
    src/node/bootstrap/epoch_bulk_pull_client.h
    src/node/bootstrap/epoch_bulk_push_client.cpp 
    src/node/bootstrap/epoch_bulk_push_client.h
    src/node/bootstrap/bootstrap.cpp
    src/node/bootstrap/bootstrap.h
    src/node/active_elections.cpp
    src/node/active_elections.h
    src/node/bootstrap/bootstrap_listener.cpp
    src/node/bootstrap/bootstrap_listener.h)


SET (ACTIVE_NETWORK germ_live_network CACHE STRING "Selects which network parameters are used")
set_property (CACHE ACTIVE_NETWORK PROPERTY STRINGS germ_test_network germ_beta_network germ_live_network)

if (GERMBLOCKS_TEST)
    add_executable (core_test
        src/core_test/block.cpp
        src/core_test/block_store.cpp
        src/core_test/interface.cpp
        src/core_test/conflicts.cpp
        src/core_test/daemon.cpp
        src/core_test/entry.cpp
        src/core_test/gap_cache.cpp
        src/core_test/ledger.cpp
        src/core_test/network.cpp
        src/core_test/node.cpp
        src/core_test/message.cpp
        src/core_test/message_parser.cpp
        src/core_test/processor_service.cpp
        src/core_test/peer_container.cpp
        src/core_test/rpc.cpp
        src/core_test/uint256_union.cpp
        src/core_test/versioning.cpp
        src/core_test/wallet.cpp
        src/core_test/wallets.cpp
        src/core_test/work_pool.cpp
        src/core_test/send_tx.cpp)

    add_executable (slow_test
        src/slow_test/node.cpp)
    
    set_target_properties (core_test slow_test PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DGERMBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DGERMBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
    set_target_properties (core_test slow_test PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
endif (GERMBLOCKS_TEST)

if (GERMBLOCKS_GUI)

    qt5_add_resources(RES resources.qrc)


    add_library (qt
        src/qt/qt.cpp
        src/qt/qt.hpp)

    add_executable (nano_wallet ${PLATFORM_GUI_TARGET_PROPERTIES}
        ${PLATFORM_WALLET_SOURCE}
        src/germ_wallet/entry.cpp
        ${RES})

    add_executable (qt_test
        src/qt_test/entry.cpp
        src/qt_test/qt.cpp)

    add_executable (qt_system
        src/qt_system/entry.cpp)

    set_target_properties (nano_wallet qt_test PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
    
    set_target_properties (qt_test qt nano_wallet qt_system PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DGERMBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DGERMBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
    set_target_properties (qt qt_system PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")
endif (GERMBLOCKS_GUI)

add_executable (germ_node
    src/germ_node/daemon.cpp
    src/germ_node/daemon.hpp
    src/germ_node/entry.cpp)

set_target_properties (argon2 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS}")
set_target_properties (blake2 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS} -D__SSE2__")
set_target_properties (ed25519 PROPERTIES COMPILE_FLAGS "${PLATFORM_C_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DED25519_CUSTOMHASH -DED25519_CUSTOMRNG")
set_target_properties (secure node germ_node germ_lib germ_lib_static PROPERTIES COMPILE_FLAGS "${PLATFORM_CXX_FLAGS} ${PLATFORM_COMPILE_FLAGS} -DQT_NO_KEYWORDS -DACTIVE_NETWORK=${ACTIVE_NETWORK} -DGERMBLOCKS_VERSION_MAJOR=${CPACK_PACKAGE_VERSION_MAJOR} -DGERMBLOCKS_VERSION_MINOR=${CPACK_PACKAGE_VERSION_MINOR} -DBOOST_ASIO_HAS_STD_ARRAY=1")
set_target_properties (secure node germ_node PROPERTIES LINK_FLAGS "${PLATFORM_LINK_FLAGS}")

if (WIN32)
    set (PLATFORM_LIBS Ws2_32 mswsock iphlpapi ntdll)
else (WIN32)
    set (PLATFORM_LIBS pthread ${CMAKE_DL_LIBS})
endif (WIN32)

if (WIN32)
    set (PLATFORM_WALLET_LIBS Qt5::WinExtras)
else (WIN32)
    set (PLATFORM_WALLET_LIBS)
endif (WIN32)

if (GERMBLOCKS_TEST)
    target_link_libraries (core_test node secure lmdb ed25519 germ_lib_static argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest_main gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})

    target_link_libraries (slow_test node secure lmdb ed25519 germ_lib_static argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest_main gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})
endif (GERMBLOCKS_TEST)

if (GERMBLOCKS_GUI)
    target_link_libraries (qt_test node secure lmdb ed25519 germ_lib_static qt argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets Qt5::Test ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS})

    target_link_libraries (qt_system node secure lmdb ed25519 germ_lib_static qt argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} gtest libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS})

    target_link_libraries (nano_wallet node secure lmdb ed25519 germ_lib_static qt argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} Qt5::Core Qt5::Gui Qt5::Widgets ${QT_QTGUI_LIBRARY} ${PLATFORM_LIBS} ${PLATFORM_WALLET_LIBS})
endif (GERMBLOCKS_GUI)

target_link_libraries (germ_lib ed25519 xxhash blake2 ${CRYPTOPP_LIBRARY})
target_link_libraries (germ_lib_static ed25519 xxhash blake2 ${CRYPTOPP_LIBRARY})

target_link_libraries (germ_node node secure lmdb ed25519 germ_lib_static argon2 ${OPENSSL_LIBRARIES} ${CRYPTOPP_LIBRARY} libminiupnpc-static ${Boost_ATOMIC_LIBRARY} ${Boost_CHRONO_LIBRARY} ${Boost_LOG_SETUP_LIBRARY} ${Boost_REGEX_LIBRARY} ${Boost_DATE_TIME_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_LOG_LIBRARY} ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_THREAD_LIBRARY} ${PLATFORM_LIBS})

set (CPACK_RESOURCE_FILE_LICENSE ${CMAKE_SOURCE_DIR}/LICENSE)
if (GERMBLOCKS_GUI)
    if (APPLE)
        install (TARGETS nano_wallet DESTINATION Nano.app/Contents/MacOS)
        install (FILES Info.plist DESTINATION Nano.app/Contents)
        install (FILES qt.conf DESTINATION Nano.app/Contents/Resources)
        install (DIRECTORY ${Qt5_DIR}/../../QtCore.framework DESTINATION Nano.app/Contents/Frameworks)
        install (DIRECTORY ${Qt5_DIR}/../../QtGui.framework DESTINATION Nano.app/Contents/Frameworks)
        install (DIRECTORY ${Qt5_DIR}/../../QtPrintSupport.framework DESTINATION Nano.app/Contents/Frameworks)
        install (DIRECTORY ${Qt5_DIR}/../../QtTest.framework DESTINATION Nano.app/Contents/Frameworks)
        install (DIRECTORY ${Qt5_DIR}/../../QtWidgets.framework DESTINATION Nano.app/Contents/Frameworks)
        install (FILES "${Qt5_DIR}/../../../plugins/platforms/libqcocoa.dylib" DESTINATION Nano.app/Contents/PlugIns/platforms)
        install (FILES RaiBlocks.icns DESTINATION Nano.app/Contents/Resources)
    elseif (WIN32)
        if ("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
            set (WIN_REDIST vc_redist.x64.exe)
        else ()
            set (WIN_REDIST vc_redist.x86.exe)
        endif ()
        set (CPACK_NSIS_EXTRA_INSTALL_COMMANDS "ExecWait '\\\"$INSTDIR\\\\${WIN_REDIST}\\\" /quiet /norestart'")
        set (CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/RaiBlocks.ico)
        set (CPACK_NSIS_DISPLAY_NAME "Nano")
        set (CPACK_PACKAGE_NAME "Nano_Installer")
        set (CPACK_NSIS_PACKAGE_NAME "Nano ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")
        set (CPACK_NSIS_URL_INFO_ABOUT "https://nano.org")
        set (CPACK_NSIS_CONTACT "info@nano.org")
        set (CPACK_NSIS_MENU_LINKS "nano_wallet.exe" "Nano Wallet" "https://nano.org" "Nano website")
        set (CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
        get_target_property (Qt5WindowsPlugin Qt5::QWindowsIntegrationPlugin LOCATION)
        get_filename_component (Qt5_bin_DIR ${Qt5_DIR}/../../../bin ABSOLUTE)
        install (TARGETS nano_wallet DESTINATION .)
        install (FILES ${CMAKE_CURRENT_BINARY_DIR}/${WIN_REDIST} DESTINATION .)
        install (FILES ${Qt5_bin_DIR}/libGLESv2.dll DESTINATION .)
        install (FILES ${Qt5_bin_DIR}/Qt5Core.dll DESTINATION .)
        install (FILES ${Qt5_bin_DIR}/Qt5Gui.dll DESTINATION .)
        install (FILES ${Qt5_bin_DIR}/Qt5Widgets.dll DESTINATION .)
        install (FILES ${Qt5_bin_DIR}/Qt5WinExtras.dll DESTINATION .)
        install (FILES ${Qt5WindowsPlugin} DESTINATION platforms)
    else ()
        install(TARGETS nano_wallet germ_node germ_lib
                RUNTIME DESTINATION bin
                LIBRARY DESTINATION lib
                ARCHIVE DESTINATION lib
        )
    endif ()
endif (GERMBLOCKS_GUI)

include (CPack)
