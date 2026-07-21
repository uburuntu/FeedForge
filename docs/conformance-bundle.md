# Synthetic conformance bundle

FeedForge can package its 23 independently reviewed Nasdaq TotalView-ITCH 5.0
fixtures as a portable conformance bundle. The payloads are synthetic test
vectors, not captured market data. The bundle is a software interoperability
aid; it is not exchange certification.

## Generate the bundle

Python 3.11 or newer is required because the reviewed fixture manifests are
read with the standard-library TOML parser. On a POSIX development host, run:

```sh
make conformance-bundle
```

This writes the expanded bundle and both archives below the ignored
`build/conformance/` directory. Override `CONFORMANCE_OUTPUT_DIR` only with a
path below `build/`. The equivalent CMake workflow is:

```sh
cmake --preset dev
cmake --build --preset dev --target conformance-bundle
```

The direct CMake target defaults to `build/dev/conformance/` and accepts the
cache variable `FEEDFORGE_CONFORMANCE_OUTPUT_DIR`. Generated directories,
`.tar.gz` files, and `.zip` files are build products and must not be committed.

## Bundle contract

Every archive contains one top-level directory named
`feedforge-itch50-conformance-v1`:

| Path | Meaning |
|---|---|
| `payloads/*.bin` | One valid raw payload for each of the 23 reviewed message types, in fixture order. |
| `streams/all-messages.binaryfile` | All payloads framed by two-byte big-endian lengths, followed by a zero-length end marker. |
| `expected/messages.json` | Exact decoded fields and canonical-pipeline emit or skip expectations. |
| `negative/payloads/*.bin` | Empty, unknown-type, size-minus-one, and size-plus-one decode cases. |
| `expected/negative.json` | Expected decode status and sizes for the 48 negative payloads. |
| `negative/framing/*.binaryfile` | Complete-empty, incomplete, truncated, trailing-data, unknown-type, and invalid-size streams. |
| `expected/framing.json` | Exact replay status, error, offset, consumption, and frame expectations. |
| `PROVENANCE.json` | Fixture hashes, review markers, generator hash, and locked official-source metadata. |
| `manifest.json` | Byte length and SHA-256 of every other file in the bundle. |
| `README.md`, `LICENSE.txt` | Standalone use notes and Apache License 2.0 text. |

The bundle format version is independent of the FeedForge release version.
Within format version 1, paths and JSON field meanings are stable. A breaking
layout or semantic change requires a new top-level bundle name and
`bundle_format_version`; adding a new optional case does not change existing
case meaning.

Normalized JSON uses sorted object keys, UTF-8/ASCII-compatible encoding, and a
final newline. Fixed-width ASCII expected values retain trailing spaces.
`manifest.json` intentionally omits its own hash to avoid recursive content.

## Reproducibility

The generator validates that exactly 23 contiguous, approved fixtures are
present and that discriminator, size, positive expectation, and boundary-case
metadata agree before writing output. It fully replaces the versioned expanded
directory so stale cases cannot survive regeneration.

Both archive formats have lexically ordered members, fixed modes, no user or
group names, and fixed timestamps. ZIP entries are stored without compression.
The gzip stream uses fixed headers and stored DEFLATE blocks, avoiding clock,
host, and compression-library variation. Repeated generation from identical
inputs is therefore byte-for-byte reproducible.

## Verification

`conformance.bundle_generator` generates the bundle twice, compares both
archives byte-for-byte, verifies member metadata and extraction, re-derives
every payload and BinaryFILE frame from the reviewed manifests, and checks all
manifest hashes.

`integration.conformance_bundle` consumes the generated files as C++20. Every
positive and decode-negative payload is classified by the independent ITCH
oracle and compared with the generated all-message decoder. The aggregate and
all framing cases are then checked through the runtime cursor and strict
generated replay adapter. Run both focused tests with:

```sh
ctest --test-dir build/dev -L conformance --output-on-failure
```

The official source PDFs are not redistributed. Their URLs, revisions, sizes,
and SHA-256 values come from `schemas/sources.lock.toml` and are embedded as
provenance only.
