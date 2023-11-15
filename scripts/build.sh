#!/usr/bin/env bash

set -e

usage(){
  echo "$0 - Build all cmake targets."
  echo " "
  echo "   By default, cmake is invoked. To skip the cmake step, use -i."
  echo " "
  echo "  -a Auto-format source files using clang-format"
  echo "  -c Clean and build"
  echo "  -g {Ninja,'Unix Makefiles'} Which generator to use. Default: Ninja"
  echo "  -h Display help and exit"
  echo "  -i Skip cmake configure step"
  echo "  -m {Debug,Release} Set build mode to use. Default: Debug"
  echo "  -q {clang,gcc} Which compiler to use (may require -f). Default: clang"
  echo "  -v {sil,hybrid_hil_v1} Which virtual harness configuration to use (SIL or HIL) (may require -f). Default: sil"
}

COMPILER=clang
CONFIGURE=1
MODE=Debug
CLEAN=0
GENERATOR=Ninja
RUN_CLANG_FORMAT=0
GIT_ROOT=`git rev-parse --show-toplevel`
# CONFIG_PKG_DIR=$GIT_ROOT/extern/shared_resources/config
TARGET=local
BUILD_DIR=$GIT_ROOT/build
CONFIG_INSTALL_DIR=$GIT_ROOT/install
ELROY_COMMON_MSG_DIR=$GIT_ROOT/extern/elroy_common_msg/generated
SCENARIO_RESULTS_DIR=$GIT_ROOT/results

while getopts ":ahifcg:m:q:" opt; do
  case ${opt} in
    h )
      usage
      exit 1
      ;;
    a )
      RUN_CLANG_FORMAT=1
      ;;
    c )
      CLEAN=1
      ;;
    g )
      GENERATOR=$OPTARG
      ;;
    i )
      CONFIGURE=0
      ;;
    m )
      MODE=$OPTARG
      ;;
    q )
      COMPILER=$OPTARG
      ;;
    \? )
      usage
      exit 1
      ;;
    : )
      usage
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))

# Ensure that the build mode is valid
case $MODE in
  Debug|Release) ;;
  * ) echo "Invalid build mode $MODE"; exit 1 ;;
esac

# Ensure that the compiler is valid and set up compiler flags
COMPILER_CC=""
COMPILER_CXX=""
if [ $COMPILER == gcc ]; then
  COMPILER_CC=gcc
  COMPILER_CXX=g++
elif [ $COMPILER == clang ]; then
  COMPILER_CC=clang
  COMPILER_CXX=clang++
else
  echo "Invalid compiler $COMPILER"; exit 1
fi

if [ $CLEAN = 1 ]; then
  echo "Cleaning build directories..."
  rm -rf $BUILD_DIR;
  rm -rf $CONFIG_INSTALL_DIR;
  rm -rf $ELROY_COMMON_MSG_DIR;
  rm -rf $SCENARIO_RESULTS_DIR;
fi

mkdir -p $BUILD_DIR

# Build config package dependencies
# pushd $CONFIG_PKG_DIR > /dev/null
# make install
# popd > /dev/null

pushd $BUILD_DIR > /dev/null

conan install ..

# optionally invoke cmake
if [ $CONFIGURE = 1 ]; then
  CC=$COMPILER_CC CXX=$COMPILER_CXX cmake \
    -G"$GENERATOR" \
    -DCMAKE_BUILD_TYPE=$MODE \
    $GIT_ROOT
fi

BUILD_CMD=''
case $GENERATOR in
  Ninja) BUILD_CMD=ninja ;;
  'Unix Makefiles') BUILD_CMD=make ;;
  * ) echo "Unsupported generator $GENERATOR"; exit 1
esac

# optionally auto-format source files with clang-format
if [ $RUN_CLANG_FORMAT = 1 ]; then
  $BUILD_CMD format
fi

# build it
$BUILD_CMD

echo $BUILD_CMD

popd > /dev/null

# Install batch-sim module and change terminal context to the
# poetry virtual environment
# pushd $BATCH_SIM_DIR > /dev/null
# make install;

# popd > /dev/null
