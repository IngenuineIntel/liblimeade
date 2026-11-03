# Memory-safety reproducers

Each C file is a standalone local reproducer. None is included in the normal
test target. Send-side cases use `limeade_init`, `limeade_connect`, and the
public send APIs with otherwise valid caller-owned data. Parser cases create a
loopback peer, deliver a wire packet through the library receive API, and call
the public parser; malformed packet contents are the specific trigger under
investigation. Build a reproducer with the library sources instrumented, for
example:

```sh
clang -std=gnu23 -g -O1 -fsanitize=address,undefined -Iinclude \
  tests/pocs/poc_truncated_numeric.c src/*.c -lz -pthread -o /tmp/poc
```

Replace the PoC filename as needed. Run only the reproducer being investigated;
several intentionally enter undefined behavior and may terminate abnormally.
The compressed-input case may require an instrumented zlib build for AddressSanitizer
to report the over-read inside zlib.

| Reproducer | Valid use and triggering circumstance | ASan result |
| --- | --- | --- |
| `poc_ack_size.c` | A connected client calls `limeade_send_await` while a connected loopback host receives with `limeade_recv_wait`; the normal ACK response reaches the client, whose ACK length remains zero. | UBSan confirms a zero-length VLA bound in the ACK handling path. This is undefined behavior; ASan alone did not catch the subsequent out-of-bounds read here. |
| `poc_parser_cleanup.c events` | A peer sends a correctly framed events packet with an event count but an invalid first event field; parser error cleanup can free uninitialized string pointers. | Confirmed invalid-free crash. |
| `poc_parser_cleanup.c generic` | A peer sends a correctly framed process snapshot with one altered record and a missing command terminator; parser error cleanup frees the uninitialized command pointer. | Confirmed invalid-free crash. |
| `poc_parser_cleanup.c update` | A peer sends a correctly framed process update with one altered record and a missing command terminator; parser error cleanup frees the uninitialized command pointer. | Confirmed invalid-free crash. |
| `poc_truncated_numeric.c` | A peer sends a correctly framed performance packet with a one-byte numeric marker where a 32-bit value is parsed; parsing reads the scalar payload past the packet end. | Confirmed heap-buffer-overflow. |
| `poc_zero_died.c` | A connected client supplies a valid empty process-update list (`total_died == 0`, `died == NULL`); packet population underflows the count before dereferencing the list. | Confirmed null read and crash. |
| `poc_string_boundary.c` | A connected client supplies a valid NUL-terminated close explanation of the maximum packet-body length; packet population fills the allocation before writing its row delimiter. | Confirmed heap-buffer-overflow. |
| `poc_populate_failure.c` | A connected client supplies a valid event array whose encoded packet exceeds the library limit; the send path continues after packet population has freed its buffer. | Confirmed crash while accessing invalid packet state. |
| `poc_compressed_length.c` | A peer sets compression metadata and sends a truncated compressed body; decompression receives the total packet size as its input length rather than the available body length. | No memory fault observed with the system zlib; not in the regression target. |

Run the sanitizer-confirmed cases with `make memory_safety` from this directory.
The target succeeds only when each expected sanitizer signature is observed.
The compressed-input case remains an investigation lead because system zlib did
not produce an observable memory fault in this run.

These are regression aids. Invalid frees, stale/uninitialized state, and reads
past packet bounds can depend on allocator, compiler, and zlib behavior; run
them only in an isolated instrumented build when investigating a report.