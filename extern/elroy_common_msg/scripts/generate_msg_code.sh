#!/usr/bin/env bash
#
# usage:
# $ ./generate_msg_code.sh [OPT ARG]

#halt script if any command fails
set -e

GENERATED_BASE_DIR_CPP="generated/cpp/include/elroy_common_msg"
GENERATED_BASE_DIR_PY="generated/py/elroy_common_msg"
MKVERSION_BASE="python3 ./scripts/mkversion.py"

## SCRIPT ARGUMENTS
SILENT_FLAG=false

# prints the argument or not
# @param[in] $1 string
function print() {
  if [ "$SILENT_FLAG" == true ]; then
    return
  else
    echo "$1"
  fi
}

function usage() {
  echo ""
  echo "usage: "
  echo "$ ./generate_msg_code.sh -[OPT] [OPT ARG]"
  echo "opt           description"
  echo "  h           this help verbose"
  echo "  s           silences verbose stdout prints"
  echo ""
}

function remove_elroy_common_msg_generated() {
  print "removing elroy_common_msg gen"
  rm -rf $GENERATED_BASE_DIR_CPP
  rm -rf $GENERATED_BASE_DIR_PY
}

function generate_elroy_common_msg() {
  # 19 Jan 2022: generating in-house
  pushd ./scripts >/dev/null

  # first remove generated
  remove_elroy_common_msg_generated

  print ""
  if [ "$SILENT_FLAG" == true ]; then
    python3 elroy_common_msg_generator.py --input-dir src >/dev/null
  else
    python3 elroy_common_msg_generator.py --input-dir src
  fi

  popd >/dev/null
}

function format_generated_files() {
  find generated -type f \( -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" \) -exec clang-format -i {} \;
}

function setup() {
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
  pushd "$SCRIPT_DIR" >/dev/null

  cd "$SCRIPT_DIR"

  GIT_ROOT=$(git rev-parse --show-toplevel)
  cd "$GIT_ROOT"

  mkdir -p $GENERATED_BASE_DIR_CPP
  mkdir -p $GENERATED_BASE_DIR_PY
  $MKVERSION_BASE -c "$GENERATED_BASE_DIR_CPP/auto_version.h" -p $GENERATED_BASE_DIR_PY/_version.py
}

function teardown() {
  cd ..

  popd >/dev/null
}

while getopts "hs" opt; do
  case "${opt}" in
    h) # help
      usage
      exit 1
      ;;
    \?)
      usage
      exit 1
      ;;
    s)
      SILENT_FLAG=true
      ;;
  esac
done
shift $((OPTIND - 1))

# generate messages in subshell
(
  setup

  generate_elroy_common_msg

  format_generated_files

  teardown
)
