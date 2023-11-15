#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
pushd "$SCRIPT_DIR" >/dev/null
GIT_ROOT=$(git rev-parse --show-toplevel)
BUILD_DIR=$GIT_ROOT/build
popd >/dev/null

pip3 install -r "${GIT_ROOT}/scripts/requirements.txt"

mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null
conan install ..
cmake \
	-GNinja \
	-DENABLE_TESTS=ON \
	"$GIT_ROOT"
ninja
./test/elroy_common_msg_tests
popd >/dev/null

python3 "${GIT_ROOT}/scripts/crc_utility.py" --poly 0x1F1922815 --crcmod-test

# The following command will import some of the python generated messages, ensuring at least that their basic syntax is
# correct, and will serialize/deserialize a test message.
# @todo FS-779 Properly test the python generator against the C++ generator.
python3 "${GIT_ROOT}/scripts/python_generator/serialize_test.py" >/dev/null
