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
| `PROVENANCE.json` | Fixture, schema, pipeline, generator, and source-lock hashes plus locked official-source metadata. |
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

The generator validates each fixture against the canonical ITCH schema and the
two named canonical pipelines before writing output. Message identity, size,
decoded field order, and projection decisions must agree exactly. Every expected
field value and value type is re-decoded from the raw payload using the schema's
wire offsets and widths. Fixture section and ordered page citations must match
the schema's message and field citations exactly. Wildcards are expanded in
schema wire order while discriminator and reserved fields remain
non-projectable. Review status, reviewer, byte-source marker, and ISO review
date are also checked rather than copied without interpretation.

The source lock must identify exactly the reviewed ITCH and BinaryFILE documents
with canonical URLs, retrieved status, and pinned digest, size, version, and
revision metadata. The bundled license must match the repository's pinned Apache
License 2.0 bytes. Expectations must contain only JSON-compatible values and
non-finite floating-point values are rejected. The generator fully replaces the
versioned expanded directory so stale cases cannot survive regeneration.

Both archive formats have lexically ordered members, fixed modes, no user or
group names, and fixed timestamps. ZIP entries are stored without compression.
The gzip stream uses fixed headers and stored DEFLATE blocks, avoiding clock,
host, and compression-library variation. Repeated generation from identical
inputs is therefore byte-for-byte reproducible.

## Verification

`conformance.bundle_generator` generates the bundle twice, compares both
archives byte-for-byte, and verifies member metadata and extraction. It
reconstructs every normalized message record, negative payload record, framing
record, and stream from the reviewed fixture manifests, schema, and pipelines.
It also compares the complete provenance records with the canonical inputs,
verifies generator, schema, pipeline, source-lock, and fixture hashes, checks the
pinned license and every manifest entry, and confirms that semantic projection,
review, expectation-value, source-lock, schema, and license mutations are
rejected.

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
