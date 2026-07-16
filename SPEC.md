# FeedForge v0.1 Specification

Status: Draft for implementation  
Target release: v0.1.0  
Audience: worker agents, reviewers, and maintainers  
Last updated: 2026-07-16

## 1. Normative language

The terms **MUST**, **MUST NOT**, **REQUIRED**, **SHOULD**, **SHOULD NOT**, and
**MAY** are normative. A worker may not silently weaken a MUST. If a requirement
is blocked or contradictory, the worker must stop, document the conflict, and
ask the integrator for a decision.

This document is the source of truth for v0.1. Implementation convenience,
generated suggestions, unrelated benchmark results, and speculative
optimisations do not override it.

## 2. Product statement

FeedForge is an ahead-of-time compiler for market-data decode pipelines. It
takes:

1. a declarative wire-schema description;
2. a declarative pipeline selecting source messages and fields;
3. an implementation profile;

and deterministically generates C++ that validates a message, decodes only the
selected fields, converts wire representations into stable value types, and
invokes a statically bound sink.

The v0.1 reference workload is Nasdaq TotalView-ITCH 5.0 stored in Nasdaq
BinaryFILE framing.

The release is successful when a fresh checkout can generate, compile, install,
and run a complete checked ITCH 5.0 pipeline, with audited fixtures and no
allocation or runtime polymorphism on the per-message hot path.

## 3. Required v0.1 outcome

v0.1 MUST provide:

- a compilable and installable C++20 runtime library;
- a code-generator host tool built as C++23;
- a versioned TOML schema format and parser;
- a complete current Nasdaq TotalView-ITCH 5.0 schema;
- a versioned TOML pipeline/projection format and parser;
- an explicit, versioned, serialisable intermediate representation;
- deterministic generation of self-contained C++20;
- one authoritative implementation profile: `portable_checked`;
- a zero-copy, in-memory BinaryFILE cursor;
- generated owning event value types containing only projected fields;
- a generated, statically dispatched, allocation-free decoder;
- a sink API with cooperative early stop;
- exact positive and negative fixtures for every message;
- sanitizer and fuzz targets;
- CI across the supported compiler and standard matrix;
- an end-to-end example that generates and replays a projected order-event
  pipeline.

v0.1 is a foundation for later optimisation. It MUST expose deliberate
implementation seams, but MUST NOT ship several premature versions of the same
kernel.

## 4. Explicit non-goals

The following are outside v0.1:

- live sockets, UDP multicast, SoupBinTCP, MoldUDP64, AF_XDP, DPDK, or io_uring;
- packet loss detection, retransmission, arbitration, or recovery;
- order-book reconstruction, matching, feature calculation, or strategy logic;
- CME MDP 3.0 or any second exchange protocol;
- runtime schema parsing on the decode path;
- runtime selection among ISA implementations;
- manual SIMD, compiler intrinsics, target cloning, PGO tuning, or BOLT;
- JIT compilation, LLVM libraries, MLIR, or C++ modules;
- C++26 reflection or `std::simd` in production targets;
- computed fields or arbitrary expressions in pipeline files;
- user-provided C++ or template injection into generated output;
- stable binary ABI guarantees;
- a database, capture service, backtester, or trading platform;
- performance claims in the README.

A worker MUST NOT add a non-goal merely because it appears easy.

## 5. Design principles

### 5.1 Correctness before speed

All input is untrusted. `portable_checked` validates framing and exact message
size before any field load. Unknown and malformed messages have explicit,
tested outcomes. Optimisation work begins only after v0.1.

### 5.2 Simple generated code

Generated C++ SHOULD resemble competent hand-written code. It MUST be
inspectable without understanding the compiler implementation. Avoid template
recursion, macro-generated control flow, type erasure, and clever forwarding
layers.

### 5.3 Offline complexity, minimal hot path

Schema parsing, validation, name resolution, and projection planning happen in
the host compiler. The generated hot path contains only:

1. a minimal size/discriminator check;
2. a switch over known message types;
3. selected field loads;
4. construction of a small owning event;
5. one direct sink invocation.

### 5.4 Stable semantics across implementations

Future variants may change loads, dispatch, layout, or vectorisation, but they
MUST implement the same events, results, errors, and sink ordering. A variant is
an implementation detail, not a semantic mode.

### 5.5 No undefined behaviour as an optimisation technique

The runtime MUST NOT use packed structs, pointer punning, unaligned typed
dereferences, compiler bit-fields, out-of-range pointer arithmetic, or reads
beyond a validated payload. Casting wire bytes to an integer pointer is
forbidden.

### 5.6 Exact representations

Prices stay fixed-point integers. Timestamps stay nanoseconds since midnight.
ASCII fields preserve every byte including right padding. No core type performs
implicit floating-point conversion.

### 5.7 Dependencies require justification

The generated runtime has no third-party runtime dependency. A dependency in
the host compiler or tests must be pinned, optional for consumers, and justified
in an accepted decision record. The default preference is the standard library.

## 6. Authoritative external specifications

The implementation MUST be audited against:

- Nasdaq TotalView-ITCH 5.0:
  https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
- Nasdaq BinaryFILE 1.00:
  https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/binaryfile.pdf

If an external document changes after this specification, workers do not
silently follow it. They record its URL, revision/date, checksum when practical,
and propose a schema/spec update.

`schemas/sources.lock.toml` records each authoritative URL, retrieval date,
document revision, and SHA-256. Nasdaq URLs are mutable; a changed checksum
requires human review, not an automatic fixture rewrite.

Precedence is: official protocol document, this document's explicit semantic
decisions, audited fixtures, implementation.

## 7. Language and toolchain policy

### 7.1 Split standards

The host compiler executable (`feedforgec`) MUST be C++23. It is an offline tool
and may use `std::expected`, `std::ranges`, `std::filesystem`,
`std::source_location`, `std::to_underlying`, and `std::byteswap` where they
improve clarity. It MUST NOT require poorly deployed C++23 library facilities
such as `std::print`, `std::format`, or `std::generator` in v0.1.

The runtime library and generated output MUST be strict C++20. They MUST also
compile in C++23 mode without changing public type layout or semantics on the
same compiler, standard-library ABI, architecture, and build options. No
cross-toolchain binary ABI guarantee is implied.

C++26 is isolated future work. Experimental reflection or `std::simd` may later
be separate targets, never a v0.1 dependency.

### 7.2 Minimum tools

- CMake 3.25 or newer;
- GCC 11 or newer for generated/runtime C++20;
- Clang 14 or newer for generated/runtime C++20;
- GCC 13.2 with libstdc++ 13, or Clang 17 with libc++ 17/libstdc++ 13, or
  newer for `feedforgec`;
- current AppleClang for macOS runtime/generated-code validation;
- MSVC 2022 as Tier 2 when support does not weaken Tier 1.

Tier 1 is Linux x86-64. Tier 2 is macOS arm64 and Windows x64.

v0.1 supports 64-bit hosts with `CHAR_BIT == 8`. Compile-time assertions reject
unsupported byte widths and mixed-endian implementations. A 32-bit port is not
a release target.

The build sets `CXX_EXTENSIONS OFF` and compile features per target; it does not
set one global language standard. Configure-time compile probes verify the
features actually used, because compiler version alone does not identify the
standard-library implementation.

### 7.3 Exceptions, RTTI, allocation, and blocking

- Generated code and runtime hot paths MUST compile with exceptions and RTTI
  disabled on GCC/Clang.
- Hot-path functions MUST be `noexcept`.
- Per-message decode and replay MUST allocate no dynamic memory after caller
  setup. This constrains FeedForge-owned work; a user sink remains responsible
  for its own behavior.
- Where supported, hot entry points SHOULD use feature-detected
  `[[clang::nonblocking]]`.
- RealtimeSanitizer is an optional current-Clang validation, not a minimum
  compiler requirement.

### 7.4 Portability choices

C++ modules, compiler-specific vector types, inline assembly, and non-standard
integer wire structs are prohibited in v0.1. Use feature-test macros for
standard-library variation. The runtime must be correct on little- and
big-endian hosts even if CI hardware is little-endian.

### 7.5 Recent C++ features: use and deferral

The C++20 runtime/generated layer deliberately uses concepts and `requires` for
sink/profile contracts, `std::span` for borrowed input, `std::byte` for wire
storage, `std::endian` for platform checks, constexpr algorithms, aggregate
events, and defaulted comparisons. It avoids runtime ranges pipelines and
library features whose support is weak at the minimum compilers.

The C++23 host layer uses `std::expected` for fallible compiler stages and may
use the facilities listed in Section 7.1. C++23 `std::byteswap` is useful in the
host but does not replace the C++20 portable wire loads.

C++26 reflection and data-parallel types are relevant future experiments, but
their compiler/library support and performance must be measured before they
enter a production target. They remain isolated from v0.1.

## 8. System architecture

~~~mermaid
flowchart LR
    S["Schema TOML"] --> C["feedforgec frontend"]
    P["Pipeline TOML"] --> C
    C --> I["Canonical FFIR"]
    I --> G["C++20 backend"]
    G --> H["Generated header"]
    B["BinaryFILE bytes"] --> F["BinaryFILE cursor"]
    F --> D["Generated decoder"]
    H --> D
    D --> K["Typed sink"]
~~~

The architecture has five boundaries:

1. **Schema frontend** parses and validates wire facts.
2. **Pipeline frontend** resolves source messages and projected fields.
3. **FFIR** captures fully resolved semantics independently of source syntax.
4. **C++ backend** emits implementation-profile-specific source.
5. **Runtime** supplies framing, portable loads, value types, outcomes, and
   policy concepts.

The compiler frontend, IR, and backend are separate modules even though v0.1
ships them in one executable. No backend may read TOML objects directly.

## 9. Required repository layout

~~~text
FeedForge/
├── CMakeLists.txt
├── CMakePresets.json
├── Makefile
├── LICENSE
├── README.md
├── SPEC.md
├── cmake/
│   ├── FeedForgeConfig.cmake.in
│   ├── FeedForgeGenerate.cmake
│   ├── FeedForgeOutputGuard.cmake
│   └── FeedForgeWarnings.cmake
├── include/feedforge/
│   ├── config.hpp
│   ├── flow.hpp
│   ├── result.hpp
│   ├── version.hpp
│   ├── framing/binary_file.hpp
│   ├── profile/concepts.hpp
│   ├── profile/portable_checked.hpp
│   ├── types/
│   │   ├── ascii.hpp
│   │   ├── decimal.hpp
│   │   ├── identifiers.hpp
│   │   └── timestamp.hpp
│   └── wire/load_big_endian.hpp
├── src/feedforgec/
│   ├── diagnostics.cpp
│   ├── emit_cpp.cpp
│   ├── ir.cpp
│   ├── lower.cpp
│   ├── main.cpp
│   ├── model.cpp
│   └── parse_toml.cpp
├── schemas/nasdaq/
│   └── totalview_itch_5_0.toml
├── schemas/sources.lock.toml
├── pipelines/
│   ├── all_messages.toml
│   └── order_events.toml
├── generated/include/feedforge/generated/nasdaq/
│   ├── itch50_all.hpp
│   └── itch50_order_events.hpp
├── examples/
│   └── replay_binary_file.cpp
├── tests/
│   ├── consumer/
│   ├── fixtures/itch50/
│   ├── golden/
│   ├── integration/
│   └── unit/
├── fuzz/
│   ├── fuzz_binary_file.cpp
│   ├── fuzz_decode_one.cpp
│   └── fuzz_replay.cpp
└── .github/workflows/
    ├── ci.yml
    └── fuzz-smoke.yml
~~~

Each v0.1 pipeline generates one include-order-independent header. The two
canonical headers are committed, reviewed, and validated by `check-generated`.
They are never edited by hand. User pipeline output lives below the build
directory and is not committed. A smaller header under `tests/golden/` is an
intentional emitter snapshot.

## 10. Host compiler contract

### 10.1 CLI

The executable is named `feedforgec`. At minimum:

~~~text
feedforgec compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline pipelines/order_events.toml \
  --output build/generated/order_events.hpp \
  [--dump-ir build/generated/order_events.ffir.json]

feedforgec validate \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  [--pipeline pipelines/order_events.toml]

feedforgec --version
feedforgec --help
~~~

Exit code `0` means success, `2` means invalid command/input, and `3` means an
input/output failure. No other non-zero code is a supported user-facing
contract in v0.1. Expected user errors produce a concise diagnostic on standard
error without a stack trace or abort. Machine output requested by `--dump-ir`
goes only to its file. Internal invariant failures may abort in a debug build
but are bugs.

The compiler MUST NOT access the network or environment-dependent configuration.
All inputs are explicit.

The compiler parses, validates, lowers, and renders completely before touching
an output path. The parent directory must already exist. On success it writes a
temporary sibling file and atomically replaces an existing destination where
the host filesystem supports atomic rename. On validation/lowering/emission
failure, an existing output is unchanged and no partial destination remains.

### 10.2 TOML dependency

The host compiler MAY use one mature, pinned TOML parsing dependency because
TOML is not in the C++ standard library. The dependency:

- is private to `feedforgec`;
- is never linked into `FeedForge::runtime` or generated consumers;
- supports system-package override;
- is fetched only when building the host tool from source;
- is recorded with exact version and licence.

No template engine is required. The C++ backend emits source through explicit
structured writers so escaping and determinism remain reviewable.

### 10.3 Diagnostics

Diagnostics include:

- source path;
- line and column when provided by the TOML parser;
- schema/pipeline object path;
- stable diagnostic code;
- human-readable message;
- optional single actionable hint.

Example:

~~~text
FFSCHEMA012 schemas/...toml:84:5:
message add_order field price ends at byte 37, beyond declared size 36
~~~

Diagnostic codes are source-level stable within v0.1.

Paths in diagnostics use the spelling supplied on the command line, normalised
to `/` separators. The compiler MUST NOT canonicalise them to an absolute path
in either diagnostics or generated output.

### 10.4 Source-name rules

Schema, type, message, field, pipeline, event, and profile names MUST match
`[a-z][a-z0-9_]*`. Namespace components MUST match
`[A-Za-z_][A-Za-z0-9_]*`. Names that are C++ keywords, begin with an underscore,
contain a double underscore, or collide in their scope are rejected.

v0.1 performs no implicit case conversion, punctuation replacement, or other
identifier normalisation. Generated identifiers preserve accepted source names
exactly. This rule makes name mapping deterministic and avoids two source names
silently becoming one C++ identifier.

## 11. FeedForge IR (FFIR)

FFIR version 1 is a resolved model, not a syntax tree. It contains:

- IR format version;
- schema name and protocol revision;
- schema and pipeline semantic SHA-256 fingerprints;
- generator semantic version;
- wire endian and discriminator description;
- complete known-message size table;
- selected source messages;
- resolved projected fields with physical and logical types;
- generated namespace and event identifiers;
- unknown/unselected policies;
- implementation profile and variant ID.

All references are resolved to stable IDs or canonical names. No source TOML
alias, comment, path, or ordering accident affects semantics.

`--dump-ir` writes canonical JSON:

- UTF-8;
- sorted object keys;
- stable array ordering defined by the model;
- no insignificant whitespace;
- no absolute path or timestamp;
- newline at EOF.

Canonical array order is: types by name, messages by unsigned discriminator
byte, fields by offset then name, and emitted events by unsigned source
discriminator. Projected fields retain pipeline declaration order because that
order defines generated member layout.

Canonical IR is a debugging and reproducibility contract. It is not yet a
long-term public interchange guarantee, but its `format_version` is mandatory.

The schema and pipeline fingerprints hash their canonical resolved semantic
JSON, not the original TOML bytes. Comments, whitespace, table order where
order is semantically irrelevant, and source paths therefore do not change a
fingerprint. Projection field order is semantic and does change it. FFIR uses
only integers, booleans, strings, arrays, and objects; it emits no floating-point
JSON number. Strings use RFC 8259 escaping. The compiler contains or privately
links one tested SHA-256 implementation and verifies it with published test
vectors. Fingerprint and provenance fields are omitted from their own hash
inputs.

## 12. Deterministic generation

For semantically identical schema, pipeline, profile, and compiler version, the
backend MUST produce byte-identical output regardless of:

- current working directory;
- TOML key order and emit-table order where declared non-semantic;
- hash-map iteration order;
- machine, user, locale, or timezone;
- build type;
- absolute source paths.

Generated source includes:

- FeedForge/compiler version;
- FFIR format version;
- schema fingerprint;
- pipeline fingerprint;
- profile/variant ID;
- a “do not edit” notice.

It MUST NOT include generation time, hostname, username, random IDs, or absolute
paths. The emitter formats source without running an external formatter.

Two independent runs and a SHA-256 comparison are a release test.

Every generated header declares the runtime source-API version it requires and
statically compares it with `feedforge::runtime_api_version`. A mismatch fails
compilation with an actionable message. This is source compatibility checking,
not a stable binary ABI promise.

The root build exposes `regenerate` and `check-generated`. Ordinary consumer
builds do not modify the source tree. With `FEEDFORGE_BUILD_COMPILER=OFF`, a
C++20-only consumer can build/install the runtime and committed canonical
pipelines without a C++23-capable host toolchain.

## 13. Implementation-profile seam

Generated code refers to cohesive profile concepts rather than hard-coding
every operation or exposing a combinatorial user-facing policy list.

v0.1 provides only:

~~~text
profile::portable_checked
  load_policy       = portable_shift_loads
  validation_policy = exact_size_validation
  dispatch_policy   = switch_dispatch
  delivery_policy   = direct_stack_event
  variant_id        = "portable_checked.v1"
~~~

Future profiles may add memcpy/byteswap loads, native unaligned loads, hot/cold
dispatch, ISA-specific projection, or materialised delivery. Every profile must
implement the same generated decoder concept and pass the same conformance
fixtures.

The public ergonomic API selects one cohesive profile. Individual policies
remain an expert/internal extension seam.

The profile is also a compiler-backend variant bundle. The generated
`Implementation` template parameter permits source-compatible kernel
substitution where the emitted control-flow shape is unchanged. If a future
dispatch or vectorisation experiment needs a different source shape,
`feedforgec` lowers the same resolved schema/projection into a new
profile-bearing FFIR and emits a second header under another cohesive variant
ID. Both forms expose the same event/outcome/sink semantics and run the same
fixtures. v0.1 implements and commits only `portable_checked.v1`; it does not
add dormant `if constexpr` branches for hypothetical variants.

For the source shape shared in v0.1, the C++ concept is named
`feedforge::decoder_implementation`. Its semantic shape is:

~~~cpp
template <class T>
concept decoder_implementation = requires(std::byte const* first) {
  { T::variant_id } -> std::convertible_to<std::string_view>;
  { T::template load_unsigned<1>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<2>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<4>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<6>(first) } noexcept
      -> std::same_as<std::uint64_t>;
  { T::template load_unsigned<8>(first) } noexcept
      -> std::same_as<std::uint64_t>;
};
~~~

`Width` is constrained to 1, 2, 4, 6, or 8. The decoder proves the whole message
range before calling it, so `first` has a schema-proven readable range of
`Width` bytes. `portable_checked` implements this with big-endian byte assembly.
Exact-size validation, switch dispatch, and stack delivery are emitted directly
in v0.1 rather than routed through indirection.

## 14. Schema TOML format

The schema starts with fields equivalent to:

~~~toml
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
name = "stock_locate"
kind = "uint"
width = 2
logical = "stock_locate"

[[types]]
name = "tracking_number"
kind = "uint"
width = 2
logical = "tracking_number"

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
~~~

The exact parser model may represent built-ins separately, but the source
format must stay declarative and auditable.

### 14.1 Supported physical/logical types

v0.1 MUST support:

- unsigned integers of widths 1, 2, 4, 6, and 8 bytes;
- fixed-width ASCII fields;
- fixed-point unsigned prices of width 4 or 8 with explicit scale;
- semantic aliases over those physical representations;
- explicit reserved byte ranges.

v0.1 MUST NOT support floats, signed wire values, variable-length fields,
optional fields, repeating groups, nested records, or arbitrary expressions.

### 14.2 Normative grammar

Schema format version 1 accepts only the following constructs. Unknown keys at
any level are errors; this catches misspellings instead of silently changing
wire semantics.

Top-level required keys are:

- `format_version` integer, exactly `1`;
- `name`, `protocol_version`, and `document_revision` strings;
- `wire_endian`, exactly `"big"` in format version 1;
- `discriminator_offset`, exactly `0`;
- `discriminator_width`, exactly `1`.

Optional top-level key `description` is documentation only.
`[[types]]` may be empty; at least one `[[messages]]` table is required.

The reserved built-in type names are `u8`, `u16`, `u32`, `u48`, `u64`, `alpha`,
and `reserved`. Integer built-ins have widths 1, 2, 4, 6, and 8 respectively.
`alpha` and `reserved` take their positive width from the field. A `reserved`
field participates in byte coverage but is not projectable and is excluded from
wildcard expansion.

Each `[[types]]` table defines one semantic type and has required `name`, `kind`,
and `width`. `kind` is `"uint"` or `"ascii"`. Optional `logical` is one of:

- `raw_unsigned`;
- `timestamp_ns`;
- `decimal`;
- `stock_locate`;
- `tracking_number`;
- `order_reference_number`;
- `match_number`;
- `share_count`;
- `ascii`.

If omitted, `logical` is `raw_unsigned` for `uint` and `ascii` for `ascii`.
`scale` is required only for `decimal`, is an integer in `[0, 18]`, and is
forbidden otherwise. Optional `description` is documentation only. User types
name a physical kind directly; aliases of aliases and therefore alias cycles do
not exist in format version 1.

Each `[[messages]]` table requires `name`, one-byte ASCII `type`, positive
`size`, and at least one nested `[[messages.fields]]`. Message `type` is exactly
one printable ASCII byte in `[0x21, 0x7e]`. Message size is in `[1, 65535]`.
Optional message keys are `description`, `spec_section`, and `spec_page`.

Each field requires `name`, `type`, `offset`, and `width`. `type` names a
built-in or declared semantic type. `offset` is zero-based; `width` must match a
fixed-width type and supplies the width for `alpha`/`reserved`. Optional keys
are `role`, `value`, `allowed`, `description`, `spec_section`, and `spec_page`.
`role` may be `"discriminator"` only. `value` is required only on the
discriminator and equals the enclosing message `type`. `allowed` is an array of
same-width strings used only as documentation in v0.1.

TOML strings used as names are ASCII. Integers are non-negative decimal or TOML
integer literals representable in 64 bits. Duplicate TOML keys are rejected by
the parser before model validation.

### 14.3 Schema validation

The compiler rejects:

- unsupported or missing format version;
- duplicate schema, type, message, or field names;
- a user type that shadows a reserved built-in type name;
- duplicate case-sensitive message discriminators;
- a name that violates Section 10.4 or collides in its scope;
- unsupported width, endian, physical kind, logical kind, or scale;
- negative offsets or non-positive widths;
- a fixed-width field whose declared width disagrees with its type;
- a field extending beyond its message;
- overlapping fields;
- an undeclared gap in message bytes;
- a final field/reserved range not ending at the declared message size;
- a message without exactly one discriminator, or a discriminator not at offset
  0 with width 1;
- disagreement between message type and discriminator-field value;
- incompatible semantic type reuse.

For ITCH, message bytes must be completely covered by declared fields,
including discriminator and reserved bytes. This turns accidental omissions
into compiler errors.

The ITCH schema audit, rather than generic compiler code, verifies that every
message has Stock Locate at offset 1/width 2, Tracking Number at offset 3/width
2, and Timestamp at offset 5/width 6. Future protocols are not forced to use an
ITCH common header.

Allowed-value metadata MAY be recorded for documentation but MUST NOT cause
runtime semantic rejection in v0.1. Exchanges may introduce new alpha values
without changing fixed wire layout.

## 15. Required ITCH 5.0 inventory

The schema MUST cover every message and every official field. The following
case-sensitive discriminator/size table is a release gate:

| Type | Message | Bytes |
|---:|---|---:|
| `S` | System Event | 12 |
| `R` | Stock Directory | 39 |
| `H` | Stock Trading Action | 25 |
| `Y` | Reg SHO Restriction | 20 |
| `L` | Market Participant Position | 26 |
| `V` | MWCB Decline Level | 35 |
| `W` | MWCB Status | 12 |
| `K` | IPO Quoting Period Update | 28 |
| `J` | LULD Auction Collar | 35 |
| `h` | Operational Halt | 21 |
| `A` | Add Order, no MPID | 36 |
| `F` | Add Order, MPID | 40 |
| `E` | Order Executed | 31 |
| `C` | Order Executed with Price | 36 |
| `X` | Order Cancel | 23 |
| `D` | Order Delete | 19 |
| `U` | Order Replace | 35 |
| `P` | Non-cross Trade | 44 |
| `Q` | Cross Trade | 40 |
| `B` | Broken Trade | 19 |
| `I` | Net Order Imbalance Indicator | 50 |
| `N` | Retail Price Improvement Indicator | 20 |
| `O` | Direct Listing with Capital Raise | 48 |

The lowercase `h` and the 2023 `O` message require dedicated regression tests;
older ITCH implementations commonly miss one or both.

For every message, the schema audit records:

- official section/page;
- type and exact size;
- field name, offset, width, physical type, and price scale;
- logical C++ type from Section 17.2;
- notes where the official document is ambiguous;
- fixture reviewer.

## 16. Pipeline TOML format

The pipeline describes projection and output naming, never C++ snippets:

~~~toml
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
~~~

`fields = ["*"]` selects all projectable, non-discriminator fields in wire-offset
order. Reserved fields are not projectable. v0.1 supports one output event per
selected source message. Explicit fields retain their schema names and are
emitted in the listed order; field renaming and transformations are deferred.

### 16.1 Required canonical pipelines

`all_messages.toml` selects every projectable, non-discriminator field of every
message. It is the conformance pipeline; reserved byte ranges remain validated
but are not event members.

Its exact metadata is:

~~~toml
format_version = 1
name = "itch50_all"
namespace = "feedforge::generated::nasdaq::itch50_all"
schema = "nasdaq_totalview_itch_5_0"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"
~~~

It has one emit per Section 15 message, ordered by unsigned discriminator; the
event name is that message's schema name and `fields = ["*"]`.

`order_events.toml` has these exact event/field projections:

| Source | Event | Fields in generated member order |
|---:|---|---|
| `A` | `add_order` | `stock_locate`, `timestamp`, `order_reference_number`, `buy_sell_indicator`, `shares`, `stock`, `price` |
| `F` | `add_order_mpid` | `stock_locate`, `timestamp`, `order_reference_number`, `buy_sell_indicator`, `shares`, `stock`, `price`, `attribution` |
| `E` | `order_executed` | `stock_locate`, `timestamp`, `order_reference_number`, `executed_shares`, `match_number` |
| `C` | `order_executed_with_price` | `stock_locate`, `timestamp`, `order_reference_number`, `executed_shares`, `match_number`, `printable`, `execution_price` |
| `X` | `order_cancel` | `stock_locate`, `timestamp`, `order_reference_number`, `cancelled_shares` |
| `D` | `order_delete` | `stock_locate`, `timestamp`, `order_reference_number` |
| `U` | `order_replace` | `stock_locate`, `timestamp`, `original_order_reference_number`, `new_order_reference_number`, `shares`, `price` |
| `P` | `trade` | `stock_locate`, `timestamp`, `order_reference_number`, `buy_sell_indicator`, `shares`, `stock`, `price`, `match_number` |

The order pipeline metadata is exactly the TOML preamble shown above the table.

### 16.2 Normative grammar

Pipeline format version 1 rejects unknown keys. Required top-level keys are
`format_version`, `name`, `namespace`, `schema`, `profile`,
`unknown_messages`, and `unselected_messages`. Their values follow Sections
10.4 and 16.1. `schema` must equal the parsed schema's `name`.

Every `[[emit]]` requires exactly `source`, `event`, and `fields`. `source` is a
one-byte discriminator string. `fields` is either exactly `["*"]` or a nonempty
array of unique explicit schema field names; wildcard and explicit fields cannot
be mixed. Emit table order has no semantic effect and canonical lowering sorts
by unsigned source discriminator. Comments and TOML key order have no semantic
effect.

### 16.3 Pipeline validation

The compiler rejects:

- unsupported format version or profile;
- unknown schema, source message, or field;
- duplicate source selections, event names, or fields within one projection;
- a pipeline with no emits;
- empty namespace or namespace components;
- invalid/reserved C++ identifiers;
- a projected field whose logical type has no C++20 value representation;
- an explicit projection of a discriminator or reserved field;
- unknown/unselected policies outside the v0.1 set;
- an emit with no fields.

`unknown_messages` is `error` or `skip`. `unselected_messages` is `skip` in
v0.1. Malformed known messages always error and cannot be configured away.

## 17. Wire and value primitives

### 17.1 Portable loads

`include/feedforge/wire/load_big_endian.hpp` provides constexpr, noexcept loads
for unsigned 8-, 16-, 32-, 48-, and 64-bit values.

The authoritative v0.1 implementation uses explicit byte assembly from
`std::byte`. A checked decoder:

1. verifies payload is non-empty before reading type;
2. resolves the exact known message size;
3. compares actual and expected size;
4. only then calls internal field loads.

Internal loads may assume the schema-proven range after that check. They do not
need a branch for every projected field.

No wire code may dereference an unaligned typed pointer or rely on struct
packing. A future profile may use `memcpy` plus byte swap behind the same load
concept.

### 17.2 Normalised owning types

At minimum, the runtime provides:

~~~cpp
namespace feedforge {

struct timestamp_ns {
  std::uint64_t value{};
  friend constexpr bool operator==(timestamp_ns, timestamp_ns) = default;
};

template <class Rep, int Scale>
struct decimal {
  Rep raw{};
  static constexpr int scale = Scale;
  friend constexpr bool operator==(decimal, decimal) = default;
};

template <std::size_t N>
struct ascii {
  std::array<char, N> raw{};
  [[nodiscard]] constexpr std::string_view trimmed() const noexcept;
  friend constexpr bool operator==(ascii, ascii) = default;
};

template <class Tag, class Rep>
struct integer_value {
  Rep value{};
  friend constexpr bool operator==(integer_value, integer_value) = default;
};

}  // namespace feedforge
~~~

`ascii<N>::trimmed()` removes only trailing ASCII space bytes (`0x20`). It does
not stop at NUL, trim leading spaces, validate an allowed-value list, or
allocate. `raw` preserves all `N` bytes.

`identifiers.hpp` declares distinct empty tag types and these exact aliases:

- `stock_locate = integer_value<stock_locate_tag, std::uint16_t>`;
- `tracking_number = integer_value<tracking_number_tag, std::uint16_t>`;
- `order_reference_number` is
  `integer_value<order_reference_number_tag, std::uint64_t>`;
- `match_number = integer_value<match_number_tag, std::uint64_t>`;
- `share_count = integer_value<share_count_tag, std::uint32_t>`.

The schema-to-public-C++ mapping is normative:

| Logical type | Required physical form | Exact generated C++ type |
|---|---|---|
| `raw_unsigned` | `uint`, width 1/2/4/6/8 | `std::uint8_t` / `std::uint16_t` / `std::uint32_t` / `std::uint64_t` / `std::uint64_t` |
| `ascii` | `ascii`, width `N` | `feedforge::ascii<N>` |
| `timestamp_ns` | `uint`, width 6 | `feedforge::timestamp_ns` |
| `decimal` | `uint`, width 4 or 8, scale `S` | `feedforge::decimal<std::uint32_t, S>` or `feedforge::decimal<std::uint64_t, S>` |
| `stock_locate` | `uint`, width 2 | `feedforge::stock_locate` |
| `tracking_number` | `uint`, width 2 | `feedforge::tracking_number` |
| `order_reference_number` | `uint`, width 8 | `feedforge::order_reference_number` |
| `match_number` | `uint`, width 8 | `feedforge::match_number` |
| `share_count` | `uint`, width 4 | `feedforge::share_count` |

The built-in `alpha` maps to `ascii`; `reserved` emits no event member. Any
logical/physical combination not in this table is a schema error. The generated
field's schema name does not change its type. Strong integer wrappers preserve
the exact wire integer and add no implicit numeric conversion.

`version.hpp` defines `inline constexpr std::uint32_t runtime_api_version = 1`.
This value increments whenever previously generated source would no longer
compile or preserve its documented source-level semantics against the runtime.

Protocol-specific cautions:

- the common six-byte timestamp decodes to an unsigned 64-bit value;
- `O.near_execution_time` remains a named raw unsigned 64-bit value unless the
  official document defines a stronger unit;
- `R.etp_leverage_factor` remains raw unsigned 32-bit; no decimal scale is
  invented;
- unknown alpha/code values are preserved and are not structural errors;
- `tracking_number` is not treated as a transport sequence number.

Generated events own every value and never retain a pointer, reference, span, or
view into input. They are standard-layout and trivially copyable, enforced by
generated static assertions.

### 17.3 C++20 result types

The runtime owns its small result types rather than changing the API based on
`std::expected` availability:

~~~cpp
enum class flow : std::uint8_t {
  continue_,
  stop,
};

enum class decode_status : std::uint8_t {
  emitted,
  known_unselected_skipped,
  unknown_skipped,
  stopped,
  empty_payload,
  unknown_message_type,
  invalid_message_size,
};

struct decode_outcome {
  decode_status status{};
  std::byte message_type{};
  std::uint16_t expected_size{};
  std::size_t actual_size{};

  [[nodiscard]] constexpr bool is_error() const noexcept;
  [[nodiscard]] constexpr bool is_terminal() const noexcept;
};
~~~

A selected event whose sink returns `continue_` yields `emitted`; the same event
whose sink returns `stop` yields `stopped`. A valid known but unselected message
yields `known_unselected_skipped`. An unknown type yields `unknown_skipped` or
`unknown_message_type` according to the generated pipeline policy.
`empty_payload`, `unknown_message_type`, and `invalid_message_size` are errors;
`stopped` is terminal but not an error. For empty input `message_type` is
`std::byte{0}`.
For every status, `actual_size` equals `payload.size()`; `message_type` equals
`payload[0]` when nonempty and `std::byte{0}` otherwise; `expected_size` equals
the declared size for every known type and zero for an unknown/empty payload.
`actual_size` never truncates a caller-provided span length. `is_terminal()` is
true for `stopped` and all errors; `is_error()` is true only for the three error
statuses.

A separate C++23 convenience adapter MAY expose `std::expected`, but it cannot
alter core semantics or event types.

## 18. BinaryFILE framing

BinaryFILE is two-byte big-endian payload length followed by that many payload
bytes. A zero length marks end of session. Clean end-of-input at a frame boundary
without the zero marker is syntactically valid but incomplete; a one-byte prefix
or partial declared payload is malformed, not merely incomplete.

`binary_file_cursor` is constructed over
`std::span<const std::byte>` and performs no I/O or allocation. It yields:

~~~cpp
struct frame_view {
  std::span<const std::byte> payload;
  std::uint64_t file_offset{};
  std::uint64_t ordinal{};
};
~~~

`file_offset` is the offset of the frame's two-byte length prefix and `ordinal`
is zero-based. The payload begins at `file_offset + 2`.

The cursor result uses these stable categories:

~~~cpp
enum class frame_status : std::uint8_t {
  frame,
  complete,
  incomplete,
  error,
};

enum class framing_errc : std::uint8_t {
  none,
  truncated_length_prefix,
  truncated_payload,
  trailing_data_after_end_marker,
};

struct frame_outcome {
  frame_status status{};
  framing_errc error{};
  frame_view frame{};
  std::uint64_t offset{};
};
~~~

An error result carries `framing_errc` and the offset of the offending length
prefix or trailing byte. `trailing_data_after_end_marker` is produced by strict
replay, not by the policy-neutral cursor.

`binary_file_cursor::next()` returns `frame_outcome`. For `frame`, `offset` is
the prefix offset and `frame` is populated. For `complete`, `offset` is the zero
marker offset. For `incomplete`, it is the input size. For `error`, it is the
offending offset. `consumed()` returns the bytes accepted so far, and
`remaining()` exposes bytes after a zero marker so strict replay can reject
them. Repeated calls after `complete`, `incomplete`, or `error` return the same
terminal outcome.

After a frame, `consumed()` points just past its payload. After a zero marker it
includes the two marker bytes. At clean boundary EOF it equals input size. A
truncated prefix or payload does not consume that malformed record, so
`consumed()` remains at its prefix offset. After a zero marker, `remaining()`
begins at the first trailing byte; otherwise it is empty.

Outcomes distinguish:

- frame available;
- complete end marker;
- incomplete end at byte boundary;
- truncated one-byte length prefix;
- length exceeding remaining bytes;
- trailing bytes after a complete marker in strict mode.

The cursor owns no strings. Errors include a stable code and byte offset.

Required semantics:

- zero length at offset zero is a valid empty complete session;
- empty input is a valid empty incomplete session;
- a file ending exactly after a non-zero payload is incomplete, not malformed;
- the payload length does not include its two-byte prefix;
- the cursor advances only after a complete frame is available;
- an error is sticky for the remaining lifetime of that cursor;
- frame spans remain valid only as long as the caller's input remains valid.

v0.1 has strict mode only for the replay adapter. A cursor may expose the end
marker without deciding how a higher layer treats trailing bytes.

## 19. Generated C++ contract

### 19.1 Event types

For every `[[emit]]`, the generated header defines an owning event struct in the
configured namespace. It:

- contains exactly the projected fields;
- uses FeedForge normalised types;
- is default constructible, standard-layout, and trivially copyable;
- contains no pointer, reference, span, string, vector, optional, variant, or
  heap-owning member;
- defines equality for tests;
- exposes static `std::byte source_discriminator` and
  `std::string_view event_name` constants.

### 19.2 Sink

A sink is invocable as:

~~~cpp
feedforge::flow sink(Event const&) noexcept;
~~~

for every generated event type. The generated decoder statically rejects a
missing, ambiguous, or potentially throwing sink overload. There is no virtual
sink and no `std::function`.

The event reference is valid only for the duration of that invocation. Because
the event owns its fields and is trivially copyable, a sink may copy it if it
needs a longer lifetime; retaining the reference itself is invalid.

The semantic constraint is equivalent to:

~~~cpp
{ sink(event) } noexcept -> std::same_as<feedforge::flow>;
~~~

### 19.3 Decoder

The generated API has this semantic shape:

~~~cpp
template <feedforge::decoder_implementation Implementation>
class basic_decoder {
 public:
  template <class Sink>
    requires sink_for_all_selected_events<Sink>
  [[nodiscard]] feedforge::decode_outcome
  decode_one(std::span<const std::byte> payload, Sink& sink) const noexcept;
};

using decoder = basic_decoder<feedforge::profile::portable_checked>;
~~~

The pipeline profile selects the emitted backend variant and the public
`decoder` alias. Expert tests and future same-shape kernels may instantiate
`basic_decoder<AnotherImplementation>` without regenerating. A profile that
changes emitted control-flow shape requires a separately generated header from
the same resolved schema/projection and a new profile-bearing FFIR, as specified
in Section 13.

`decode_one`:

1. checks non-empty payload;
2. identifies whether the discriminator is known;
3. obtains the exact expected size for every known type;
4. rejects size mismatch before projected-field loads;
5. skips a valid unselected known type;
6. loads only projected fields for a selected type;
7. constructs one event on the stack;
8. invokes the sink exactly once;
9. reports cooperative stop without classifying it as an error.

A known but unselected message MUST still pass exact-size validation. This
prevents projections from hiding corrupted input.

### 19.4 Replay adapter

The runtime owns the pipeline-independent replay result types, and each
generated namespace supplies a thin strict replay function combining
`binary_file_cursor` and `decoder`. Their semantic public shape is:

~~~cpp
namespace feedforge {

enum class replay_status : std::uint8_t {
  complete,
  incomplete,
  stopped,
  framing_error,
  decode_error,
};

struct replay_summary {
  replay_status status{};
  std::uint64_t frames_seen{};
  std::uint64_t events_emitted{};
  std::uint64_t known_messages_skipped{};
  std::uint64_t unknown_messages_skipped{};
  std::size_t bytes_consumed{};
  std::size_t error_offset{};
  feedforge::framing_errc framing_error{};
  feedforge::decode_outcome decode_error{};
};

}  // namespace feedforge

template <class Sink>
  requires sink_for_all_selected_events<Sink>
[[nodiscard]] feedforge::replay_summary
replay_binary_file(std::span<const std::byte> input, Sink& sink) noexcept;
~~~

Error-specific fields are inspected only when `status` names their error kind;
otherwise they remain defaulted. There is no heap-owned diagnostic text in the
summary.

Replay stops at the first error or sink stop. Counters include the event whose
sink requested stop.

`frames_seen` increments when the cursor returns a complete nonzero frame,
before its payload is decoded. Thus an unknown message, invalid ITCH message
size, or sink-stopped frame counts; a truncated BinaryFILE prefix/payload and
the zero marker do not. Once a sink stops, replay does not inspect any later
byte or search for an end marker. Strict replay always rejects any byte after a
zero marker if the marker is reached.

The replay terminal state is exactly one of `complete`, `incomplete`, `stopped`,
`framing_error`, or `decode_error`. When replay stops or errors before natural
end, it MUST NOT guess whether the session would have been complete. On natural
end, `bytes_consumed` includes every frame prefix/payload and includes the
two-byte zero marker for a complete session. On sink stop it includes the frame
whose event requested stop; on a decode error it includes that complete frame;
on a framing error it stops at the malformed record's prefix. A decode-error
byte offset identifies the first payload byte; a framing-error offset follows
Section 18.

Processing order is per frame: obtain a framing result; if it is a frame, decode
that payload; stop on its decode error or sink stop; otherwise request the next
frame. Only when no frame remains does replay classify complete versus
incomplete. Strict trailing bytes are diagnosed immediately after the zero
marker. v0.1 does not report semantic enum/value errors.

### 19.5 Compile-time checks

Generated headers statically assert:

- expected size of every known message;
- projected field bounds;
- supported width/scale combinations;
- event type traits;
- discriminator/event-name uniqueness;
- implementation concept satisfaction;
- generated/runtime source-API version equality.

These checks defend against compiler-emitter regressions and manual corruption;
the host compiler remains the primary schema validator.

## 20. Build, package, and consumer contract

The project exports:

- `FeedForge::runtime`;
- imported executable target `FeedForge::compiler` when host tools are built;
- a CMake `feedforge_generate()` function;
- generated interface targets;
- an installable `FeedForgeConfig.cmake`.

The v0.1 runtime is a header-only CMake interface library. Generated headers
depend only on C++20 standard headers and installed public `feedforge/` runtime
headers; “self-contained” does not mean independent of `FeedForge::runtime`.

Canonical artifact names are fixed:

| Pipeline | Include | Namespace | CMake target |
|---|---|---|---|
| `itch50_all` | `<feedforge/generated/nasdaq/itch50_all.hpp>` | `feedforge::generated::nasdaq::itch50_all` | `FeedForge::generated::itch50_all` |
| `itch50_order_events` | `<feedforge/generated/nasdaq/itch50_order_events.hpp>` | `feedforge::generated::nasdaq::itch50_order_events` | `FeedForge::generated::itch50_order_events` |

`feedforge_generate(NAME x ...)` creates `FeedForge::generated::x` and a header
under a target-specific build directory; it never writes into the source tree.

Illustrative consumer usage:

~~~cmake
find_package(FeedForge CONFIG REQUIRED)

feedforge_generate(
  NAME order_events
  SCHEMA "nasdaq_totalview_itch_5_0"
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/order_events.toml"
)

target_link_libraries(app
  PRIVATE
    FeedForge::runtime
    FeedForge::generated::order_events
)
~~~

`feedforge_generate` uses a custom command with complete dependencies on the
schema, pipeline, compiler executable, and relevant backend version. Incremental
builds regenerate only when inputs change. Parallel builds cannot race the same
output. `SCHEMA` accepts an explicit file path or the canonical installed ID
`nasdaq_totalview_itch_5_0`; the latter resolves to package data without network
access. Generating a custom pipeline requires `FeedForge::compiler`. A
compiler-disabled consumer can only use the installed canonical generated
targets.

Schemas install under `${CMAKE_INSTALL_DATADIR}/feedforge/schemas`, and the
package config exposes their absolute installed location as
`FeedForge_SCHEMA_DIR`. If `FeedForge::compiler` is unavailable,
`feedforge_generate()` fails during configure with a message directing the user
to install/enable the compiler or use a canonical target.

Required project options:

| Option | Default | Meaning |
|---|---:|---|
| `FEEDFORGE_BUILD_COMPILER` | top-level ON | build `feedforgec` |
| `FEEDFORGE_BUILD_TESTS` | top-level ON | unit/integration tests |
| `FEEDFORGE_BUILD_EXAMPLES` | top-level ON | examples |
| `FEEDFORGE_BUILD_FUZZERS` | OFF | libFuzzer targets |
| `FEEDFORGE_WARNINGS_AS_ERRORS` | CI ON | project targets only |
| `FEEDFORGE_ENABLE_ASAN` | OFF | AddressSanitizer |
| `FEEDFORGE_ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer |
| `FEEDFORGE_ENABLE_RTSAN` | OFF | supported Clang only |

The runtime advertises `cxx_std_20`. The compiler advertises `cxx_std_23`.
`CXX_EXTENSIONS` is `NO` for all project targets. Project targets MUST NOT force
warnings, sanitizer flags, exception policy, RTTI policy, or optimisation flags
onto consumers.

At least these presets are documented:

- `fast`: compiler and focused unit tests, Debug, no examples/fuzzers;
- `dev`: compiler, tests, examples, Debug;
- `release`: compiler, tests, examples, Release;
- `sanitizers`: Clang ASan+UBSan;
- `fuzz`: Clang libFuzzer targets.

The root `Makefile` MAY provide a self-documenting developer command surface
over these presets and targets. It MUST remain an optional convenience wrapper:
CMake owns the build graph, CTest owns test registration, consumers do not
require Make, and a Make target MUST NOT change runtime/compiler semantics or
bypass the generation, packaging, benchmark, or release gates in this document.

## 21. Testing strategy

### 21.1 Test independence

Fixture bytes and expected decoded values MUST be hand-authored from the
official specification or independently reviewed. Tests MUST NOT derive all
expected bytes from the same schema/emitter/load functions under test.

Each fixture records:

- official specification section/page;
- raw hex bytes;
- expected message and fields;
- expected canonical pipeline event;
- author and independent reviewer.

The author and reviewer MUST be different human or worker identities. A second
worker may provide the independent review; self-approval metadata is invalid.
The reviewer checks the raw hex and expected values directly against the cited
official table, not only against generated output.

Binary fixture files MAY be generated from reviewed hex text by a tiny test
utility, provided the reviewed hex remains the source of truth.

### 21.2 Runtime unit tests

Required:

- big-endian load widths 1, 2, 4, 6, and 8;
- zero, all-ones, alternating, and boundary values;
- 48-bit timestamp;
- fixed-point raw preservation;
- ASCII padding and non-allocating trimming;
- flow/result helpers;
- event type traits;
- feature-detection paths compiling in C++20 and C++23.

### 21.3 Compiler tests

Every schema/pipeline validation rule has a focused positive or negative test.
Also test:

- exact source-name acceptance and rejection from Section 10.4;
- C++ reserved keyword and collision rejection;
- safe C++ string/identifier emission;
- canonical FFIR JSON;
- identical semantic fingerprints/output after comments, TOML key order, and
  emit-table order are changed;
- deterministic source output across repeated runs and working directories;
- diagnostics without absolute paths or unstable data;
- golden output compile in C++20 and C++23.

### 21.4 BinaryFILE tests

Required:

- empty incomplete file;
- empty complete file;
- one and multiple frames;
- complete and incomplete session;
- zero-length marker after frames;
- truncated one-byte prefix;
- truncated payload;
- maximum 16-bit length;
- bytes after end marker in strict replay;
- cursor offsets and ordinals;
- exact `consumed()` and `remaining()` at every terminal state;
- sticky error;
- cooperative replay stop.

### 21.5 ITCH conformance tests

For every message in Section 15:

- at least one valid all-fields fixture;
- successful `all_messages` decode;
- exact projected field values;
- size-minus-one rejection;
- size-plus-one rejection;
- valid skip under `order_events` for every source unselected by that pipeline;
- exact emission under `order_events` for each selected
  `A`/`F`/`E`/`C`/`X`/`D`/`U`/`P` source.

Cross-message cases:

- lowercase `h` versus uppercase `H`;
- current `O` message;
- unknown discriminator under error and skip pipelines;
- empty payload;
- multi-frame complete and incomplete files;
- known unselected malformed message still errors;
- sink stop and exact summary counters.

### 21.6 Projection and compile-fail tests

Tests prove:

- projected event structs contain no unrequested member;
- generated source does not emit loads for unrequested fields;
- all selected event types require sink overloads;
- missing or throwing sink overloads fail compilation with useful diagnostics;
- event layout is identical in C++20 and C++23 modes on the same compiler,
  standard-library ABI, architecture, and options;
- error/skip/stop semantics do not depend on profile internals.

An emitted-source structural test is acceptable for the “no unrequested load”
v0.1 gate. Later optimisation phases replace it with disassembly/benchmark
evidence.

An instrumented test implementation counts `load_unsigned` calls. Empty,
unknown, size-minus-one, size-plus-one, and malformed known-unselected payloads
must perform zero field-load calls. This is the executable proof that size
validation precedes field access; fuzz output alone cannot establish ordering.

### 21.7 Allocation and real-time tests

A test counts global allocations during replay after input and sink setup and
observes zero. Another target builds core/generated code with exceptions and
RTTI disabled. If current Clang supports RTSan, a smoke target exercises the
generated hot path.

### 21.8 Fuzzing

Required Clang libFuzzer targets:

1. arbitrary bytes through `binary_file_cursor`;
2. arbitrary payload through the all-message `decode_one`;
3. arbitrary BinaryFILE through cursor plus decoder and no-op sink.

Seed corpora contain every valid fixture and every framing/decode error class.

Properties:

- no crash, UB, out-of-bounds access, hang, or uncontrolled allocation;
- deterministic outcome for identical bytes;
- no sink call after an error;
- at most one sink call per payload;
- no field load before exact-size validation.

CI runs a short fixed-duration smoke. Longer fuzzing is scheduled/manual.

### 21.9 Installed consumer test

CI installs FeedForge to a temporary prefix, configures a separate minimal
project with `find_package`, generates a pipeline, builds it as C++20, and runs
it. Source-tree-only success is insufficient.

## 22. CI matrix

The matrix is:

| Job | Standard | Gate |
|---|---|---|
| GCC minimum Linux runtime/generated | C++20 | compiler OFF; committed headers, unit, integration, install |
| Clang minimum Linux runtime/generated | C++20 | compiler OFF; committed headers, unit, integration, install |
| current GCC full project | C++20 runtime + C++23 compiler | build/test |
| current GCC generated validation | C++23 | generated code compatibility |
| current Clang full project | C++20 runtime + C++23 compiler | build/test |
| current Clang generated validation | C++23 | generated code compatibility |
| current Clang ASan+UBSan | mixed | all tests |
| current Clang no exceptions/RTTI | C++20 | runtime/generated build/test |
| macOS AppleClang | C++20 generated/runtime | build/test/example |
| Windows MSVC | C++20 generated/runtime | Tier 2 build/test |
| installed consumer | C++20 | package/generation/run |
| fuzz smoke | C++20 | seed regression |

Warnings are errors for project source in CI. Generated code is held to the same
warning level as hand-written runtime code.

Hosted CI is correctness infrastructure only. It MUST NOT publish latency or
throughput comparisons.

All Linux jobs, installed generation, sanitizers, and fuzz smoke are Tier 1 and
block v0.1. macOS arm64 and Windows x64 are Tier 2: they are expected green, but
the owner may temporarily waive a failure only with a linked issue, failure
analysis, last-known-green commit, and an explicit release-note limitation.
Runner unavailability is recorded separately from a product failure. A Tier 2
waiver never permits weakening Tier 1 semantics or public API.

The minimum-compiler jobs configure `FEEDFORGE_BUILD_COMPILER=OFF` and consume
the committed canonical headers. The installed-generation job uses a current
C++23 host toolchain and installs with the compiler enabled.

## 23. Documentation requirements

Before release:

- README defines the boundary, states “offline checked v0.1”, and includes a
  five-minute build/generate/replay path;
- `docs/architecture.md` explains frontend, FFIR, backend, runtime, and profiles;
- schema and pipeline formats are documented independently of code;
- generated API documentation covers sink, result, skip/error, and lifetime
  semantics;
- `docs/testing.md` explains fixture provenance and fuzzing;
- `docs/adding-a-backend.md` shows the future variant seam without implementing
  a second backend;
- limitations state that FeedForge is experimental and not exchange-certified
  or production trading infrastructure.

Generated source MUST NOT be the only documentation of its API.

## 24. Worker-agent execution protocol

### 24.1 General rules

Every worker MUST:

1. read this entire specification and its assigned work package;
2. inspect the current repository before assuming a dependency is unfinished;
3. state assumptions in its PR/handoff;
4. edit only owned files unless the integrator approves a shared-file change;
5. add tests in the same change as behavior;
6. run package checks plus the `fast` preset;
7. report exact commands and results;
8. avoid unrelated formatting, renames, dependencies, or refactors;
9. never manually edit generated source;
10. leave the branch buildable and generated output current.

If a worker proposes a new public API, source format, dependency, compiler floor,
or semantic behavior, it first proposes a SPEC/ADR change and waits for approval.

### 24.2 Shared-file rule

One integration owner edits:

- root `CMakeLists.txt` and `CMakePresets.json`;
- package configuration;
- root public naming;
- CI workflows;
- canonical generated-output refreshes;
- task sequencing and release audit.

Parallel workers describe required shared-file edits to the integrator instead
of racing them. Two workers MUST NOT edit the same generated package or fixture
manifest concurrently.

### 24.3 Simplicity rule

For v0.1 prefer:

- explicit portable byte assembly over architecture-specific loads;
- a generated switch over dispatch metaprogramming;
- owning small events over lifetime-sensitive wire views;
- explicit IR structs over a plugin framework;
- standard library code over a new dependency;
- small duplicated emitted fragments over an opaque abstraction.

“Faster” without the future benchmark protocol is not justification for
complexity.

### 24.4 Handoff requirements

Each PR/handoff includes:

- intent and design summary;
- files changed;
- tests added;
- commands run and outcomes;
- known limitations;
- SPEC deviations, ideally none;
- whether canonical generation changed and why.

No worker may claim a performance improvement during v0.1.

## 25. Work-package dependency graph

~~~mermaid
flowchart TD
    W0["FF-000 Integrator scaffold"] --> W1["FF-100 Runtime primitives"]
    W0 --> W2["FF-200 Compiler frontend and FFIR"]
    W1 --> W3["FF-300 BinaryFILE"]
    W2 --> W4["FF-400 ITCH schema"]
    W1 --> W5["FF-500 C++ backend"]
    W2 --> W5
    W3 --> W6["FF-600 End-to-end integration"]
    W4 --> W6
    W5 --> W6
    W6 --> W7["FF-700 Hardening"]
    W7 --> W8["FF-800 Release audit"]
~~~

Freeze FFIR and the runtime profile/sink/result contracts after FF-200 and
FF-100 are integrated. Renderer and protocol work may then proceed in parallel.

### FF-000 — Repository and build scaffold

Owner: integrator  
Dependencies: none  
Owned files: root build files, `cmake/`, directory skeleton, CI skeleton

Deliverables:

- runtime and compiler targets with split C++20/C++23 requirements;
- options and presets from Section 20;
- test discovery and warning policy;
- install/export skeleton;
- licence and formatting/static-analysis configuration;
- `FEEDFORGE_BUILD_COMPILER=OFF` path.

Gate:

- trivial runtime and compiler tests pass;
- runtime-only C++20 configure/build works;
- installable target names exist;
- no flags leak to a tiny consumer.

### FF-100 — Runtime wire and value primitives

Dependencies: FF-000  
Owned files: `include/feedforge/wire/`, `types/`, `flow.hpp`, `result.hpp`,
`profile/concepts.hpp`, feature/attribute compatibility headers, unit tests

Deliverables:

- portable big-endian loads;
- normalised owning types;
- flow/decode/error values;
- decoder-implementation and reusable sink concept primitives;
- compile-time platform assertions;
- optional guarded nonblocking attribute.

Gate:

- all boundary/endian/type-trait tests pass;
- warning-free C++20 and C++23 builds;
- no-exception/no-RTTI target passes;
- a test-only implementation can satisfy the profile concept without editing
  replay/orchestration code.

### FF-200 — Compiler frontend and FFIR

Dependencies: FF-000  
Owned files: `src/feedforgec/` except final C++ emitter, compiler unit fixtures

Deliverables:

- command line and version reporting;
- TOML source models;
- schema/pipeline parsers;
- stable diagnostics;
- every validation rule;
- lowering to resolved FFIR;
- canonical JSON dump and fingerprints.

Gate:

- minimal valid schema/pipeline lowers;
- every invalid category has exact diagnostic-code coverage;
- repeated and cross-directory FFIR dumps are byte-identical;
- compiler code is warning-free C++23.

### FF-300 — BinaryFILE framing

Dependencies: FF-000 and FF-100 result primitives  
Owned files: `include/feedforge/framing/binary_file.hpp`, framing tests and
framing fuzz seed corpus

Deliverables:

- zero-copy cursor;
- complete/incomplete/error semantics;
- offsets, ordinals, and sticky error.

Gate:

- every Section 21.4 case passes;
- arbitrary input smoke passes ASan/UBSan;
- cursor performs no dynamic allocation.

### FF-400 — Complete ITCH schema and audited fixtures

Dependencies: FF-200 source format/validator frozen  
Owned files: `schemas/nasdaq/`, `schemas/sources.lock.toml`,
`tests/fixtures/itch50/`, schema audit notes

Deliverables:

- every official field of all 23 messages;
- complete byte coverage and semantic annotations;
- one hand-authored valid fixture per message;
- expected decoded values and spec citations;
- independent review metadata;
- size-negative cases.

Gate:

- compiler validation succeeds;
- all Section 15 sizes match;
- no field is inferred only from another open-source parser;
- lowercase `h` and current `O` are present;
- official source checksums/retrieval date are recorded.

The schema worker MUST NOT implement the C++ decoder. This separation makes its
review meaningful.

### FF-500 — Deterministic C++ backend and profile seam

Dependencies: FF-100 and FF-200  
Owned files: compiler emitter module,
`include/feedforge/profile/portable_checked.hpp`, emitter tests,
`tests/golden/`

Deliverables:

- generated event structs;
- known tag/size metadata;
- selected/unselected switch decoder;
- direct sink invocation and stop propagation;
- provenance/fingerprint comments;
- generated static assertions;
- concrete `portable_checked` profile and generated `basic_decoder`/`decoder`
  integration.

Gate:

- deterministic source hash;
- synthetic projections compile in exact C++20 and C++23;
- generated code is warning-free and no-exception/no-RTTI compatible;
- compile-fail sink tests are useful;
- a test-only profile passes the same synthetic semantics without orchestration
  edits.

### FF-600 — Canonical pipelines and end-to-end integration

Dependencies: FF-300, FF-400, FF-500  
Owned files: `pipelines/`, canonical generated packages, `examples/`,
integration tests; integrator applies shared CMake edits

Deliverables:

- `all_messages` and `order_events`;
- replay adapter and summary;
- strict trailing-data behavior in the generated replay adapter;
- BinaryFILE replay example;
- `feedforge_generate()`;
- `regenerate` and `check-generated`;
- source-tree and installed-consumer flows.

Gate:

- every audited message replays through `all_messages`;
- projected values and emit/skip counts are exact;
- canonical generated files are clean under `check-generated`;
- external C++20 consumer generates and runs a pipeline;
- runtime-only consumer builds with compiler disabled.

### FF-700 — Hardening

Dependencies: FF-600  
Owned files: fuzz targets/corpora, negative and compile-fail tests, allocation
test, test documentation; integrator applies CI edits

Deliverables:

- ASan/UBSan and fuzz smoke;
- allocation instrumentation;
- no-exception/no-RTTI validation;
- complete malformed-input suite;
- optional RTSan smoke;
- full CI matrix.

Gate:

- required CI is green;
- no sanitizer suppression hides a project-code defect;
- all v0.1 error/status branches have tests;
- fuzz corpus includes every valid fixture.

### FF-800 — Release audit and documentation

Owner: integrator/release worker  
Dependencies: FF-700  
Owned files: README, docs, release notes, cross-cutting release fixes

Deliverables:

- requirement-to-test traceability;
- clean-clone and install verification;
- schema audit confirmation;
- docs from Section 23;
- v0.1 release notes and known limitations.

Gate: every Definition of Done item below has recorded evidence.

## 26. Definition of Done for v0.1

v0.1 is done only when all statements are true:

- [ ] A clean clone configures, builds, and tests with documented presets.
- [ ] Runtime/generated public targets have zero third-party dependencies.
- [ ] `feedforgec` builds as C++23 and can be disabled.
- [ ] A compiler-disabled C++20 build installs runtime and canonical pipelines.
- [ ] Schema/pipeline validation emits actionable stable diagnostics.
- [ ] FFIR and generated C++ are byte-deterministic.
- [ ] Generated provenance has version/fingerprints but no host path/time.
- [ ] The complete 23-message current ITCH 5.0 schema passes field audit.
- [ ] Every message has independently reviewed valid and invalid fixtures.
- [ ] The checked decoder validates exact size for every known message.
- [ ] Known unselected messages are validated before being skipped.
- [ ] Selected messages load only projected fields into owning trivial events.
- [ ] BinaryFILE complete, incomplete, malformed, and stopped states are distinct.
- [ ] Replay and a no-op sink allocate zero memory after setup.
- [ ] Runtime/generated code builds without exceptions and RTTI.
- [ ] Required C++20, C++23, Linux, macOS Arm, and Tier 2 jobs meet policy.
- [ ] ASan, UBSan, and fuzz smoke are green.
- [ ] Canonical generated packages pass `check-generated`.
- [ ] Source-tree and installed external consumer examples work.
- [ ] README makes no unsupported production or latency claim.
- [ ] No optimisation-only dependency or architecture-specific hot-path branch
  entered v0.1.

## 27. Reviewer rejection checklist

Reject code that:

- treats a packed C++ struct as a wire message;
- reads before validating the required range;
- implicitly converts prices to floating point;
- retains event references into caller input;
- uses heap allocation, exceptions, RTTI, locks, virtual dispatch, or
  `std::function` in per-message decode;
- silently accepts an incorrect fixed message length;
- decodes every field despite a narrower projection;
- skips validation for known but unselected messages;
- semantically rejects a new alpha/code value;
- hand-edits generated source;
- couples event semantics/layout to an optimisation profile;
- adds an avoidable runtime dependency;
- applies ISA flags to baseline/runtime targets;
- modifies benchmark-facing code despite benchmarks being outside this release;
- calls FeedForge production-grade or exchange-certified.

## 28. Deferred optimisation roadmap

The architecture leaves room for future work in this order:

1. Freeze a benchmark contract, hardware manifests, and holdout corpora.
2. Add an independent simple reference decoder if it was not needed for v0.1.
3. Add alternative load profiles: `memcpy`/byteswap and native unaligned.
4. Add dispatch alternatives while preserving event/error semantics.
5. Add runtime function multiversioning and x86-64/Arm selection.
6. Experiment with C++26 `std::simd` and explicit intrinsics only for
   demonstrably homogeneous work.
7. Add MoldUDP64 framing, sequence tracking, and explicit gap events as another
   runtime layer.
8. Import SBE XML and add CME MDP 3.0 as the second proof of generality.
9. Add optional materialisation, SPSC, order-book, or feature consumers outside
   the compiler/runtime core.

Every future profile runs the v0.1 conformance fixtures. Optimisation may change
implementation, code size, compile time, or latency; it may not change decoded
values, framing state, error classes, or sink order.

## 29. Optional real-data integration

Full-day exchange data is not downloaded in ordinary CI. An opt-in integration
command may validate a user-supplied or separately downloaded BinaryFILE.

A published real-data result records:

- source URL/provider and licence constraints;
- compressed and decompressed SHA-256;
- FeedForge commit;
- schema/compiler fingerprints;
- total frames and per-type counts;
- end-marker presence;
- exact command and machine manifest.

No exchange dataset is committed unless redistribution rights are explicit.

## 30. Owner inputs and resolved repository settings

Resolved on 2026-07-10:

- GitHub repository: `uburuntu/FeedForge`;
- default branch: `main`;
- current visibility: private.

Visibility is an owner-controlled repository setting, not a v0.1 code gate. A
future public launch does not change the technical Definition of Done.

These remain repository-owner decisions, not worker decisions:

- Apache-2.0 versus another permissive licence;
- whether Windows remains Tier 2 if it delays v0.1;
- which pinned TOML and test-only libraries are acceptable.

Defaults pending an explicit answer:

- Apache-2.0;
- Windows Tier 2 may be temporarily non-blocking with a tracked issue;
- the integrator chooses the smallest mature dependencies satisfying Sections
  10 and 21.

## 31. Reference material for implementers

- Nasdaq TotalView-ITCH 5.0 specification:
  https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHSpecification.pdf
- Nasdaq BinaryFILE specification:
  https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/binaryfile.pdf
- GCC C++ standard support:
  https://gcc.gnu.org/projects/cxx-status.html
- Clang C++ standard support:
  https://clang.llvm.org/cxx_status.html
- CMake compile-feature model:
  https://cmake.org/cmake/help/latest/prop_gbl/CMAKE_CXX_KNOWN_FEATURES.html
- Clang RealtimeSanitizer:
  https://clang.llvm.org/docs/RealtimeSanitizer.html
- Clang libFuzzer:
  https://llvm.org/docs/LibFuzzer.html
- C++26 data-parallel types proposal:
  https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p1928r15.pdf
