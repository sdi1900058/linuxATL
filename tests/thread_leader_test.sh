#!/bin/bash
set -euo pipefail

ROOT_DIR="/home/jortzis/leitourgika/linuxATL"
BUILD_DIR="${ROOT_DIR}/tests"
SPAWNER_SRC="${BUILD_DIR}/thread_spawner.c"
SPAWNER_BIN="${BUILD_DIR}/thread_spawner"
K22TEST_BIN="${ROOT_DIR}/k22test"
TMP_OUTPUT="$(mktemp)"

cleanup() {
	if [[ -n "${SPAWNER_PID:-}" ]]; then
		kill "${SPAWNER_PID}" 2>/dev/null || true;
		wait "${SPAWNER_PID}" 2>/dev/null || true;
	fi
	rm -f "${TMP_OUTPUT}"
}
trap cleanup EXIT

if [[ ! -x "${K22TEST_BIN}" ]]; then
	echo "error: ${K22TEST_BIN} not found or not executable" >&2
	exit 1
fi

if [[ ! -f "${SPAWNER_SRC}" ]]; then
	echo "error: ${SPAWNER_SRC} not found" >&2
	exit 1
fi

echo "[+] Building thread spawner helper..."
gcc -pthread "${SPAWNER_SRC}" -o "${SPAWNER_BIN}"

echo "[+] Launching thread spawner..."
"${SPAWNER_BIN}" &
SPAWNER_PID=$!
sleep 1

echo "[+] Running k22test..."
"${K22TEST_BIN}" > "${TMP_OUTPUT}"

echo "[+] Validating results..."
MATCH_COUNT=$(grep -c "thread_spawner," "${TMP_OUTPUT}" || true)

if [[ "${MATCH_COUNT}" -ne 1 ]]; then
	echo "error: expected exactly one entry for thread_spawner, found ${MATCH_COUNT}" >&2
	echo "----- k22test output -----"
	cat "${TMP_OUTPUT}"
	exit 1
fi

echo "[+] Success: k22tree reported a single entry for the multi-threaded process."

