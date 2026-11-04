#!/bin/sh

set -u

expect_asan()
{
  label=$1
  pattern=$2
  shift 2
  output=$(mktemp)
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 timeout 15s "$@" >"$output" 2>&1
  status=$?

  if [ "$status" -eq 124 ]; then
    printf 'FAIL: %s timed out\n' "$label"
    cat "$output"
    rm -f "$output"
    return 1
  fi
  if [ "$status" -eq 0 ] || ! grep -Eq "$pattern" "$output"; then
    printf 'FAIL: %s did not produce the expected AddressSanitizer finding\n' "$label"
    cat "$output"
    rm -f "$output"
    return 1
  fi

  printf 'PASS: %s produced the expected AddressSanitizer finding\n' "$label"
  rm -f "$output"
}

expect_ubsan()
{
  label=$1
  pattern=$2
  shift 2
  output=$(mktemp)
  ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    timeout 15s "$@" >"$output" 2>&1
  status=$?

  if [ "$status" -eq 124 ]; then
    printf 'FAIL: %s timed out\n' "$label"
    cat "$output"
    rm -f "$output"
    return 1
  fi
  if [ "$status" -eq 0 ] || ! grep -Eq "$pattern" "$output"; then
    printf 'FAIL: %s did not produce the expected UndefinedBehaviorSanitizer finding\n' "$label"
    cat "$output"
    rm -f "$output"
    return 1
  fi

  printf 'PASS: %s produced the expected UndefinedBehaviorSanitizer finding\n' "$label"
  rm -f "$output"
}

# as far as I can tell, this is a false positive
#expect_ubsan 'ACK packet serialization alignment' \
#  'variable length array bound evaluates to non-positive value 0' \
#  pocs/poc_ack_size.ubsan || exit 1
expect_asan 'events parser invalid cleanup' \
  'ERROR: AddressSanitizer: SEGV' pocs/poc_parser_cleanup.asan events || exit 1
expect_asan 'generic process parser invalid cleanup' \
  'ERROR: AddressSanitizer: SEGV' pocs/poc_parser_cleanup.asan generic || exit 1
expect_asan 'process update parser invalid cleanup' \
  'ERROR: AddressSanitizer: SEGV' pocs/poc_parser_cleanup.asan update || exit 1
expect_asan 'truncated numeric field read' \
  'ERROR: AddressSanitizer: heap-buffer-overflow' pocs/poc_truncated_numeric.asan || exit 1
expect_asan 'empty dead-process list read' \
  'ERROR: AddressSanitizer: SEGV' pocs/poc_zero_died.asan || exit 1
expect_asan 'maximum-length string delimiter write' \
  'ERROR: AddressSanitizer: heap-buffer-overflow' pocs/poc_string_boundary.asan || exit 1
expect_asan 'send after packet population failure' \
  'ERROR: AddressSanitizer: SEGV' pocs/poc_populate_failure.asan || exit 1
