#!/bin/bash
set -euo pipefail

ROOT_DIR="/home/Desktop/linuxATL"
TEST_DIR="${ROOT_DIR}/tests"
STRESS_SRC="${TEST_DIR}/k22tree_stress.c"
STRESS_BIN="${TEST_DIR}/k22tree_stress"

if [[ ! -f "${STRESS_SRC}" ]]; then
	echo "error: ${STRESS_SRC} not found" >&2
	exit 1
fi

echo "[+] Building k22tree stress tester..."
gcc -pthread "${STRESS_SRC}" -o "${STRESS_BIN}"

echo "[+] Running stress test with args: $*"
"${STRESS_BIN}" "$@"

