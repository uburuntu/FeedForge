# FeedForge architecture

> FeedForge is experimental, is not exchange-certified, and is not production
> trading infrastructure.

FeedForge is an offline, ahead-of-time compiler for checked market-data decode
pipelines. The host tool consumes a wire schema, a projection pipeline, and one
cohesive implementation profile. It resolves them into FeedForge IR (FFIR) and
emits C++20. At runtime, framing bytes pass through the generated decoder to a
statically bound typed sink; no schema or pipeline file is parsed there.

This document defines the component boundaries and public design contract.
Released availability is described by the artifacts, generated headers, and
[test coverage](testing.md) in the same revision.

## Data flow

```text
Schema TOML ----> schema frontend --+
                                    +--> resolved FFIR --> C++ backend --> generated header
Pipeline TOML --> pipeline frontend-+                         ^
                                                              |
                                                    cohesive profile/variant

BinaryFILE bytes --> runtime cursor --> generated decoder --> typed sink
```

The offline host compiler is C++23. The public runtime and generated output are
strict C++20 and must also compile in C++23 mode without changing their
documented semantics. Generated headers depend on the public FeedForge runtime
headers, but have no third-party runtime dependency.

## Boundary contracts

### Schema frontend

The schema frontend owns wire facts. It parses the versioned
[schema format](schema-format.md), validates names, physical/logical type
combinations, complete byte coverage, discriminator placement, field bounds,
and overlap, then produces resolved schema data. It does not select output
events or emit C++.

All input is untrusted. For the checked v0.1 design, exact message size is
established before any projected field load. Allowed-value metadata documents
known codes; it does not make a structurally valid message fail because an
exchange introduced a new code.

### Pipeline frontend

The pipeline frontend owns projection and output naming. It parses the
versioned [pipeline format](pipeline-format.md), resolves each source
discriminator and field against the schema, expands wildcards, checks policies
and C++ identifiers, and preserves explicit projected-field order. It accepts
no C++ snippets, expressions, renames, or runtime behavior.

Schema and pipeline parsing are separate concerns even when they share TOML
infrastructure. A valid schema can be validated without a pipeline; a pipeline
cannot be lowered without its named schema.

### FFIR

FFIR version 1 is a fully resolved semantic model, not a TOML syntax tree. It
contains the format version, schema identity and revision, semantic
fingerprints, generator version, endian/discriminator facts, the complete
known-message size table, selected messages, resolved projected fields and
types, generated names, message policies, and profile/variant identity.

The backend receives FFIR only; it must not inspect frontend TOML objects.
Canonical FFIR JSON is deterministic debugging and reproducibility output:
UTF-8, sorted keys, defined array order, no insignificant whitespace, no host
paths or timestamps, and a final newline. It is versioned but is not a
long-term public interchange guarantee.

### C++ backend

The backend turns resolved FFIR into one include-order-independent generated
header per pipeline. It owns source spelling and control-flow shape, while FFIR
owns semantics. Output includes compiler/runtime compatibility data, FFIR
version, schema and pipeline fingerprints, profile/variant identity, and a
do-not-edit notice. It excludes generation time, user, host, random IDs, and
absolute paths.

For semantically identical inputs, profile, and compiler version, output must
be byte-identical across working directories and machines. Generated source is
never edited by hand. See [the generated API contract](generated-api.md).

### Runtime

The runtime is protocol- and pipeline-independent. It supplies:

- big-endian unsigned loads for widths 1, 2, 4, 6, and 8;
- owning value types such as `timestamp_ns`, `decimal`, `ascii`, and strong
  integer identifiers;
- `flow`, decode outcomes, and replay summaries;
- the allocation-free, I/O-free `binary_file_cursor`;
- sink and decoder-implementation concepts; and
- the authoritative `profile::portable_checked` implementation.

The runtime does not know schema message names, projected event sets, or user
sinks. Generated events own decoded values; borrowed frame spans remain tied to
the caller-owned input. Source compatibility is checked with
`feedforge::runtime_api_version`; v0.1 does not promise a stable binary ABI.

### Profiles and backend variants

A profile is one cohesive implementation choice, not a public mix-and-match
list of load, validation, dispatch, and delivery policies. v0.1 defines only
`feedforge::profile::portable_checked`, with variant ID
`portable_checked.v1`: portable byte-assembly loads, exact-size validation,
switch dispatch, and direct stack-event delivery.

The generated `basic_decoder<Implementation>` seam permits a same-source-shape
implementation satisfying `feedforge::decoder_implementation`. Any variant that
changes emitted control flow must be emitted as a distinct, profile-bearing
backend variant from the same resolved semantics. Every variant must preserve
event types, values, errors, skip/stop behavior, and sink order.

## Deliberate limits

The v0.1 boundary excludes live networking, packet recovery, order-book or
strategy logic, runtime schema parsing, runtime ISA selection, a second
exchange protocol, computed pipeline fields, and user C++ injection. FeedForge
validates and decodes an offline BinaryFILE workload; it does not certify data,
provide trading controls, or constitute a trading platform.
