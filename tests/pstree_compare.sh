#!/bin/bash
set -euo pipefail

ROOT_DIR="/home/Desktop/linuxATL"
K22TEST_BIN="${ROOT_DIR}/k22test"
TMP_K22="$(mktemp)"
TMP_PSTREE="$(mktemp)"

cleanup() {
	rm -f "${TMP_K22}" "${TMP_PSTREE}"
}
trap cleanup EXIT

if [[ ! -x "${K22TEST_BIN}" ]]; then
	echo "error: ${K22TEST_BIN} not found or not executable" >&2
	exit 1
fi

if ! command -v pstree >/dev/null 2>&1; then
	echo "error: pstree command not found" >&2
	exit 1
fi

echo "[+] Capturing pstree output..."
pstree -p > "${TMP_PSTREE}"

echo "[+] Running k22test..."
"${K22TEST_BIN}" > "${TMP_K22}"

missing=0
total=0

while IFS= read -r line; do
	[[ "${line}" =~ ^# ]] && continue
	if [[ -z "${line}" ]]; then
		continue
	fi

	IFS=',' read -r comm pid _ <<< "${line}"
	if [[ -z "${comm}" || -z "${pid}" ]]; then
		continue
	fi

	label="${comm}(${pid})"
	if ! grep -qF "${label}" "${TMP_PSTREE}"; then
		echo "[-] Missing in pstree: ${label}"
		((missing++))
	fi
	((total++))
done < "${TMP_K22}"

echo "[+] Compared ${total} processes."

if [[ "${missing}" -gt 0 ]]; then
	echo "error: ${missing} processes reported by k22tree not found in pstree output." >&2
	exit 1
fi

echo "[+] Success: every k22tree entry appears in pstree output."

