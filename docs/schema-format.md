# Schema TOML format

> **Status:** This is the normative format-version-1 design from SPEC Sections
> 10, 14, and 17, not a statement that a particular build has complete parser
> or lowering support. Availability requires the corresponding compiler and
> release tests. FeedForge remains experimental and is not exchange-certified.

A schema records fixed wire layout and semantic value types. It does not select
events, contain C++, or configure runtime processing. A separate
[pipeline](pipeline-format.md) selects messages and fields.

## Example

```toml
format_version = 1
name = "nasdaq_totalview_itch_5_0"
protocol_version = "5.0"
document_revision = "2023-04-28"
wire_endian = "big"
discriminator_offset = 0
discriminator_width = 1

[[types]]
name = "timestamp"
kind = "uint"
width = 6
logical = "timestamp_ns"

[[types]]
name = "price_4"
kind = "uint"
width = 4
logical = "decimal"
scale = 4

[[messages]]
name = "system_event"
type = "S"
size = 12
spec_section = "System Event Message"
spec_page = 9

[[messages.fields]]
name = "message_type"
type = "alpha"
offset = 0
width = 1
role = "discriminator"
value = "S"

[[messages.fields]]
name = "stock_locate"
type = "stock_locate"
offset = 1
width = 2

[[messages.fields]]
name = "tracking_number"
type = "tracking_number"
offset = 3
width = 2

[[messages.fields]]
name = "timestamp"
type = "timestamp"
offset = 5
width = 6

[[messages.fields]]
name = "event_code"
type = "alpha"
offset = 11
width = 1
allowed = ["O", "S", "Q", "M", "E", "C"]
```

## Lexical and identifier rules

Unknown keys at every level are errors. Duplicate TOML keys are rejected before
model validation.

Schema, type, message, and field names must match
`[a-z][a-z0-9_]*`. Names are ASCII, case-sensitive, and preserved exactly; the
compiler performs no case folding or punctuation replacement. A name is
rejected if it is a C++ keyword, begins with `_`, contains `__`, or collides
with another name in its scope.

Message discriminators are exactly one printable ASCII byte from `0x21` through
`0x7e`, and are case-sensitive. TOML integers must be non-negative and
representable in 64 bits; model-specific ranges below apply after parsing.

## Top-level grammar

The required top-level keys are:

- `format_version`: integer, exactly `1`;
- `name`: schema identifier;
- `protocol_version`: string;
- `document_revision`: string;
- `wire_endian`: exactly `"big"`;
- `discriminator_offset`: exactly `0`; and
- `discriminator_width`: exactly `1`.

`description` is the only optional top-level key and is documentation-only.
Zero or more `[[types]]` tables may appear. At least one `[[messages]]` table is
required.

## Physical and logical types

The reserved built-in names are:

- `u8`, `u16`, `u32`, `u48`, and `u64`: unsigned integers of 1, 2, 4, 6, and 8
  bytes respectively;
- `alpha`: fixed-width ASCII, with width supplied by the field; and
- `reserved`: an explicit byte range, with width supplied by the field.

A reserved field participates in complete byte coverage but cannot be
projected and is omitted from wildcard expansion.

Each `[[types]]` table requires:

- `name`: a unique non-built-in identifier;
- `kind`: `"uint"` or `"ascii"`; and
- `width`: the fixed positive byte width.

`logical` is optional. Its default is `raw_unsigned` for `uint` and `ascii` for
`ascii`. The accepted logical names and physical combinations are:

- `raw_unsigned`: `uint` of width 1, 2, 4, 6, or 8;
- `timestamp_ns`: `uint` of width 6;
- `decimal`: `uint` of width 4 or 8;
- `stock_locate`: `uint` of width 2;
- `tracking_number`: `uint` of width 2;
- `order_reference_number`: `uint` of width 8;
- `match_number`: `uint` of width 8;
- `share_count`: `uint` of width 4; and
- `ascii`: fixed-width `ascii`.

`scale` is required for `decimal`, must be an integer from 0 through 18, and is
forbidden for every other logical type. `description` is optional and
documentation-only. A user type names a physical kind directly; format version
1 has no aliases of aliases and therefore no alias cycles.

Format version 1 has no floats, signed wire integers, variable-length fields,
optional fields, repeating groups, nested records, or expressions.

## Message and field grammar

Each `[[messages]]` table requires:

- `name`: unique message identifier;
- `type`: unique, case-sensitive, printable one-byte discriminator;
- `size`: integer from 1 through 65535; and
- one or more nested `[[messages.fields]]` tables.

The optional message keys are `description` (string), `spec_section` (string),
and `spec_page` (non-negative integer); they are audit/documentation metadata
and do not affect decoding.

SPEC Section 14.2 names those metadata keys but does not explicitly state their
TOML value types. The types above record the current frontend interpretation;
the source specification should make them explicit before the format is frozen.

Each nested field requires:

- `name`: unique within its message;
- `type`: a built-in or declared semantic type;
- `offset`: zero-based byte offset; and
- `width`: positive byte width.

For a fixed-width built-in or declared type, `width` must agree with that type.
For `alpha` and `reserved`, the field supplies the width.

Optional field keys are:

- `role`, whose only value is `"discriminator"`;
- `value`, required only for the discriminator and equal to the enclosing
  message `type`;
- `allowed`, an array of strings having the field's width; and
- `description` and `spec_section` strings, and a non-negative integer
  `spec_page`, as audit metadata.

`allowed` documents codes only. A checked decoder preserves unknown alpha/code
bytes and does not semantically reject them in v0.1.

## Validation

Before lowering, the compiler must reject:

- missing or unsupported format version, endian, kind, logical type, width, or
  scale;
- duplicate names or case-sensitive message discriminators;
- a user type shadowing a built-in;
- invalid/reserved identifiers or scope collisions;
- negative offsets, non-positive widths, or a width/type disagreement;
- a field extending beyond its declared message;
- overlapping fields, undeclared gaps, or coverage not ending exactly at
  `size`;
- a message without exactly one discriminator at offset 0 and width 1;
- disagreement between message `type` and discriminator `value`; and
- an incompatible logical/physical type combination.

Every byte of an ITCH message must be covered by a field or explicit reserved
range. The generic grammar does not force ITCH's Stock Locate, Tracking Number,
or Timestamp common header; that protocol-specific rule belongs to the ITCH
schema audit.

## Semantics, provenance, and determinism

The authoritative order is the official protocol document, explicit decisions
in [SPEC.md](../SPEC.md), independently audited fixtures, then implementation.
`schemas/sources.lock.toml` is required to record source URL, retrieval date,
document revision, and SHA-256. A changed upstream checksum requires human
review, not an automatic schema or fixture rewrite.

Comments, source paths, TOML key order, and table order where declared
non-semantic do not affect the resolved schema fingerprint. Types are
canonicalized by name, messages by unsigned discriminator, and fields by offset
then name. The fingerprint hashes canonical resolved semantic JSON, not the
original TOML bytes. See [Architecture](architecture.md) for the FFIR boundary.
