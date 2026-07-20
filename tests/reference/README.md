# Independent ITCH decode oracle

`itch50_oracle.hpp` is a standard-library-only reference implementation. It
contains a separately transcribed table of the 23 ITCH message discriminators
and sizes, an independent big-endian byte loop, and status classification. It
does not include FeedForge, read generated metadata, or parse the canonical
schema.

`itch50_differential.hpp` is the adapter to the system under test. Its 23 sink
overloads compare every projected event member with a hard-coded protocol
offset and width. `matches_generated()` checks both continue and stop delivery.

`itch50_oracle_self_test.cpp` exercises the reference primitives, all status
branches, fixed boundary patterns, deterministic random patterns, and a
single-byte mutation at every payload position. It can be built without adding
it to the project graph:

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion \
  -Werror -fno-exceptions -fno-rtti \
  -Iinclude -Igenerated/include \
  tests/reference/itch50_oracle_self_test.cpp \
  -o build/itch50-oracle-self-test
```

`fuzz/fuzz_differential_decode.cpp` exposes
`feedforge_fuzz_differential_decode_input()` and `LLVMFuzzerTestOneInput`. Each
input is checked directly, then transformed into every valid message size so
that arbitrary field values for all 23 events are exercised in every run.

## Independence limit

The oracle is implementation-independent, not provenance-independent. Its
message facts and field offsets are a second manual transcription of the same
public protocol specification used to audit the canonical schema. It can catch
an emitter, loader, dispatch, size, or generated-header defect when the two
transcriptions differ, but a specification misunderstanding repeated in both
will survive. A reviewer should audit this transcription directly against the
protocol document rather than against the schema or generated C++.

The adapter necessarily names generated event types and public data members to
observe the system under test. It does not use their generated discriminator
constants or metadata as expected values.
