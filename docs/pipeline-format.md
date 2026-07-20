# Pipeline TOML format

This document normatively defines pipeline format version 1. An implementation
conforms only if it accepts, rejects, resolves, and fingerprints pipelines as
specified here.

A pipeline selects schema messages and fields, names generated events, chooses
one cohesive profile, and sets unknown/unselected message policy. It is
independent of the [wire schema](schema-format.md): it cannot change offsets,
types, sizes, or allowed wire values. It contains no C++ snippets, field
renames, computed fields, or arbitrary expressions.

## Example

```toml
format_version = 1
name = "itch50_order_events"
namespace = "feedforge::generated::nasdaq::itch50_order_events"
schema = "nasdaq_totalview_itch_5_0"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"

[[emit]]
source = "A"
event = "add_order"
fields = [
  "stock_locate",
  "timestamp",
  "order_reference_number",
  "buy_sell_indicator",
  "shares",
  "stock",
  "price",
]

[[emit]]
source = "X"
event = "order_cancel"
fields = [
  "stock_locate",
  "timestamp",
  "order_reference_number",
  "cancelled_shares",
]
```

## Identifiers

Pipeline and event names must match `[a-z][a-z0-9_]*`. Schema references and
explicit field names use their exact accepted schema spelling; there is no case
conversion or punctuation normalization. Namespace components must match
`[A-Za-z_][A-Za-z0-9_]*`, and the namespace must contain at least one
nonempty component.

Names are ASCII and case-sensitive. C++ keywords, names beginning with `_`,
names containing `__`, and collisions in a generated scope are rejected.
Message discriminators are case-sensitive one-byte strings; for example,
lowercase `"h"` and uppercase `"H"` identify different ITCH messages.

## Top-level grammar

Format version 1 rejects unknown keys. Every top-level key is required:

- `format_version`: integer, exactly `1`;
- `name`: pipeline identifier;
- `namespace`: `::`-separated valid C++ namespace components;
- `schema`: exactly the parsed schema's `name`;
- `profile`: `"portable_checked"` in v0.1;
- `unknown_messages`: `"error"` or `"skip"`; and
- `unselected_messages`: exactly `"skip"` in v0.1.

At least one `[[emit]]` table is required.

## Emit grammar

Each `[[emit]]` table has exactly three required keys:

- `source`: a one-byte discriminator string naming a schema message;
- `event`: a unique generated event identifier; and
- `fields`: either exactly `["*"]` or a nonempty array of unique, explicit
  schema field names.

Wildcard and explicit names cannot be mixed. `["*"]` expands to all projectable,
non-discriminator fields in wire-offset order. Explicit fields retain their
schema names and listed order; this order is semantic because it defines
generated member layout. Discriminator and `reserved` fields are not
projectable.

Format version 1 permits one output event per selected source message. Emit
table order itself is not semantic: canonical lowering sorts emits by the
unsigned discriminator byte. Comments and TOML key order are also
non-semantic.

## Validation

Against the selected schema, the compiler must reject:

- a missing/unsupported format version or profile;
- an unknown or mismatched schema name;
- an unknown source discriminator or projected field;
- duplicate source selections or event names;
- duplicate explicit fields within one projection;
- no emits, an empty field list, a wildcard resolving to no projectable fields,
  or a mixed wildcard/explicit list;
- an empty/invalid namespace or invalid/reserved C++ identifier;
- projection of a discriminator or reserved field;
- a projected logical type without the required C++20 value representation;
  and
- unknown/unselected policy values outside the v0.1 set.

Malformed known messages always produce a decode error. Neither policy may
disable exact-size validation:

- a selected known message is validated and then emitted;
- an unselected known message is validated and then returns
  `known_unselected_skipped`;
- an unknown discriminator returns `unknown_message_type` under `"error"` or
  `unknown_skipped` under `"skip"`.

See [Generated API](generated-api.md) for complete outcome and replay semantics.

## Canonical v0.1 pipelines

The required [`itch50_all` pipeline](../pipelines/all_messages.toml) uses namespace
`feedforge::generated::nasdaq::itch50_all`, schema
`nasdaq_totalview_itch_5_0`, profile `portable_checked`,
`unknown_messages = "error"`, and `unselected_messages = "skip"`. It has one
emit for every message declared by the named schema, sorted by unsigned
discriminator. Each event uses the schema message name and `fields = ["*"]`.
This is the conformance pipeline; reserved ranges remain validated but are not
event members.

The required [`itch50_order_events` pipeline](../pipelines/order_events.toml) uses namespace
`feedforge::generated::nasdaq::itch50_order_events` and the same schema,
profile, and policies. Its exact projections, in generated member order, are:

- `A` → `add_order`: `stock_locate`, `timestamp`,
  `order_reference_number`, `buy_sell_indicator`, `shares`, `stock`, `price`;
- `F` → `add_order_mpid`: the `A` fields plus `attribution`;
- `E` → `order_executed`: `stock_locate`, `timestamp`,
  `order_reference_number`, `executed_shares`, `match_number`;
- `C` → `order_executed_with_price`: `stock_locate`, `timestamp`,
  `order_reference_number`, `executed_shares`, `match_number`, `printable`,
  `execution_price`;
- `X` → `order_cancel`: `stock_locate`, `timestamp`,
  `order_reference_number`, `cancelled_shares`;
- `D` → `order_delete`: `stock_locate`, `timestamp`,
  `order_reference_number`;
- `U` → `order_replace`: `stock_locate`, `timestamp`,
  `original_order_reference_number`, `new_order_reference_number`, `shares`,
  `price`; and
- `P` → `trade`: `stock_locate`, `timestamp`, `order_reference_number`,
  `buy_sell_indicator`, `shares`, `stock`, `price`, `match_number`.

## Lowering and fingerprints

Lowering resolves wildcard fields, schema references, logical C++ types,
message sizes, policies, generated names, and profile/variant ID into FFIR.
Source paths, comments, TOML key order, and emit-table order do not enter the
pipeline semantic fingerprint. Explicit field order does. The C++ backend reads
that resolved FFIR rather than pipeline TOML; see
[Architecture](architecture.md).
