#!/bin/bash

qt_dir=${1}
src_dir=${2}

set -o errexit
set -o nounset
set -o xtrace
OS=`uname`

mkdir build
pushd build

if [[ ${ASAN_INT-0} -eq 1 ]]; then
    SANITIZERS="-DGERMBLOCKS_ASAN_INT=ON"
elif [[ ${ASAN-0} -eq 1 ]]; then
    SANITIZERS="-DGERMBLOCKS_ASAN=ON"
elif [[ ${TSAN-0} -eq 1 ]]; then
    SANITIZERS="-DGERMBLOCKS_TSAN=ON"
else
    SANITIZERS=""
fi

cmake \
    -G'Unix Makefiles' \
    -DACTIVE_NETWORK=germ_test_network \
    -DGERMBLOCKS_TEST=ON \
    -DGERMBLOCKS_GUI=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_VERBOSE_MAKEFILE=ON \
    -DBOOST_ROOT=/usr/local \
    -DQt5_DIR=${qt_dir} \
    ${SANITIZERS} \
    ..


if [[ "$OS" == 'Linux' ]]; then
    cmake --build ${PWD} -- -j2
else
    sudo cmake --build ${PWD} -- -j2
fi

popd

if [[ "$OSTYPE" == "darwin"* ]]; then
    TRUE_CMD=gtrue
else
    TRUE_CMD=true
fi

pushd load-tester
cargo build --release
popd
cp ./load-tester/target/release/raiblocks-load-tester ./build/load_test

./ci/test.sh ./build || ${TRUE_CMD}
