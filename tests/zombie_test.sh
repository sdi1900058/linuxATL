#!/bin/bash
set -euo pipefail

ROOT_DIR="/home/Desktop/linuxATL"
TEST_DIR="${ROOT_DIR}/tests"
HELPER_SRC="${TEST_DIR}/zombie_helper.c"
HELPER_BIN="${TEST_DIR}/zombie_helper"
K22TEST_BIN="${ROOT_DIR}/k22test"
TMP_OUTPUT="$(mktemp)"
INFO_FILE="$(mktemp)"

cleanup() {
	if [[ -n "${HELPER_PID:-}" ]]; then
		kill "${HELPER_PID}" 2>/dev/null || true
		wait "${HELPER_PID}" 2>/dev/null || true
	fi
	rm -f "${TMP_OUTPUT}" "${INFO_FILE}"
}
trap cleanup EXIT

if [[ ! -x "${K22TEST_BIN}" ]]; then
	echo "error: ${K22TEST_BIN} not found or not executable" >&2
	exit 1
fi

if [[ ! -f "${HELPER_SRC}" ]]; then
	echo "error: ${HELPER_SRC} not found" >&2
	exit 1
fi

echo "[+] Building zombie helper..."
gcc "${HELPER_SRC}" -o "${HELPER_BIN}"

echo "[+] Spawning zombie process..."
"${HELPER_BIN}" > "${INFO_FILE}" &
HELPER_PID=$!

# Wait for helper to report PIDs
for _ in {1..50}; do
	if [[ -s "${INFO_FILE}" ]]; then
		break
	fi
	sleep 0.1
done

if [[ ! -s "${INFO_FILE}" ]]; then
	echo "error: helper did not report PIDs" >&2
	exit 1
fi

# shellcheck disable=SC1090
source "${INFO_FILE}"

if [[ -z "${child_pid:-}" || -z "${parent_pid:-}" ]]; then
	echo "error: failed to parse helper output" >&2
	exit 1
fi

echo "[+] Helper parent PID: ${parent_pid}, zombie PID: ${child_pid}"

echo "[+] Running k22test..."
"${K22TEST_BIN}" > "${TMP_OUTPUT}"

echo "[+] Searching for zombie entry..."
MATCH_COUNT=$(grep -c "k22_zombie,${child_pid}," "${TMP_OUTPUT}" || true)

if [[ "${MATCH_COUNT}" -lt 1 ]]; then
	echo "error: expected zombie process PID ${child_pid} in k22test output" >&2
	echo "----- k22test output -----"
	cat "${TMP_OUTPUT}"
	exit 1
fi

echo "[+] Success: zombie process was reported by k22tree."

