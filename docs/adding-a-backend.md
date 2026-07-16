# Adding a future backend variant

> **Scope:** The v0.1 contract admits only the cohesive `portable_checked` /
> `portable_checked.v1` design. This guide documents the future extension seam;
> it does not assert generated-backend availability or propose/implement a
> second backend. FeedForge is experimental, not exchange-certified, and not
> production trading infrastructure.

Read [Architecture](architecture.md), [Generated API](generated-api.md), and
SPEC Sections 5, 11–13, 19, 21, and 28 first. A new variant may change how code
is implemented, but not what consumers observe.

## Terms

- A **frontend** parses and validates schema/pipeline source.
- **FFIR** is the only resolved semantic input to a backend.
- A **backend variant** emits a particular C++ source/control-flow shape.
- A **decoder implementation** supplies same-shape primitives such as unsigned
  loads to an already-emitted `basic_decoder`.
- A **profile** is the cohesive user-facing bundle of load, validation,
  dispatch, delivery, and backend choices.

Do not expose individual policies as a combinatorial public API. One profile
name maps to one reviewed bundle and one stable variant ID.

## Decide whether a new backend is needed

If the candidate only substitutes the unsigned-load primitive while preserving
the emitted v0.1 control flow, test it through the existing concept:

```cpp
struct candidate_implementation {
  static constexpr std::string_view variant_id = "candidate.v1";

  template <std::size_t Width>
  [[nodiscard]] static std::uint64_t
  load_unsigned(std::byte const* first) noexcept;
};

static_assert(
    feedforge::decoder_implementation<candidate_implementation>);
```

The decoder has already validated the complete message range before calling
`load_unsigned`; `Width` is one of 1, 2, 4, 6, or 8. This same-shape experiment
can instantiate a generated `basic_decoder<candidate_implementation>` in tests.
It does not justify a second emitter or dormant `if constexpr` branches in the
authoritative v0.1 header.

A distinct backend variant is required when dispatch, delivery,
materialisation, vectorisation, or another choice changes emitted source shape.
That variant is emitted from the same resolved schema/projection as a separate
profile-bearing FFIR/output, not selected dynamically inside the v0.1 decoder.

## Invariants every variant preserves

A backend must not:

- read TOML models or redo frontend name/type resolution;
- change event names, projected members, member order, public value types, or
  owning/lifetime rules;
- change exact-size validation, including validation of known unselected
  messages;
- read a field before validating the complete known message size;
- change unknown, skip, error, stop, replay-counter, offset, or sink-order
  semantics;
- semantically reject an unknown alpha/code value;
- introduce runtime schema parsing, runtime polymorphism, or user C++ injection;
- retain input or event references beyond their documented call lifetime; or
- weaken C++20, `noexcept`, no-exception/no-RTTI, portability, or
  FeedForge-owned allocation contracts.

The frontend remains authoritative for source validation. The generated
compile-time assertions remain defense against emitter regressions and manual
corruption, not a replacement for frontend validation.

## Change sequence

1. **Specify the variant.** Before code, obtain an accepted SPEC/ADR change
   defining the cohesive profile name, unique stable `variant_id`, source
   shape, compatibility boundary, and why the existing seam is insufficient.
   Decide explicitly whether accepting the profile requires a new pipeline
   format version.
2. **Keep FFIR complete.** Add only the resolved profile/variant information
   the backend needs. Do not pass paths, TOML nodes, aliases, comments, or
   frontend ordering accidents through a side channel. Update canonical FFIR
   serialization and its version if compatibility requires it.
3. **Implement the primitive or emitter.** For a same-shape load
   implementation, satisfy `feedforge::decoder_implementation`. For a
   different source shape, add a structured emitter variant that consumes FFIR
   and emits direct, inspectable C++.
4. **Select cohesively.** Map the accepted pipeline profile to the full variant
   bundle during lowering. Do not add independent public switches for load,
   validation, dispatch, and delivery.
5. **Preserve provenance.** Generated output records compiler version, FFIR
   version, schema/pipeline fingerprints, profile/variant ID, runtime API
   requirement, and do-not-edit notice. It contains no timestamp, host/user
   identity, random ID, or absolute path.
6. **Package separately.** Give a different-shape generated artifact an
   unambiguous header/target identity so it cannot overwrite the authoritative
   canonical output. Source-tree generated files remain machine-produced and
   are updated only through regeneration.
7. **Review compatibility.** If old generated source no longer compiles or no
   longer preserves documented source semantics against the runtime, increment
   `feedforge::runtime_api_version` and provide an actionable static assertion.
   Do not imply binary ABI stability.
8. **Document availability.** Update format, architecture, generated API,
   consumer, and release-limit documentation only when artifacts and tests
   actually exist. Do not describe an experiment as released or certified.

## Required evidence

Run the candidate through the same independently reviewed fixtures as
`portable_checked`, including all valid ITCH messages and every malformed,
unknown, unselected, stop, and framing case. Compare observable events,
outcomes, replay summaries, counters, offsets, and sink order.

Also require:

- focused tests for every added FFIR and emitter branch;
- deterministic FFIR/source across repeated runs and working directories;
- generated compilation in required C++20 and C++23 modes;
- missing/throwing sink compile-fail tests;
- an instrumented proof that no field load precedes exact-size validation;
- ASan, UBSan, no-exception/no-RTTI, and applicable RTSan coverage;
- all three fuzz layers with reviewed fixtures in their seed corpora;
- zero FeedForge-owned allocation after replay setup with a no-op sink;
- source-tree and installed external-consumer tests; and
- canonical `check-generated` evidence when a committed variant is approved.

No variant is acceptable merely because it compiles or produces the same happy
path. Semantic parity, malformed-input behavior, determinism, and lifetime
safety are release gates. Do not make performance claims before the separate
benchmark contract and holdout protocol in SPEC Section 28 exist.
