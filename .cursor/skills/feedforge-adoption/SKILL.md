---
name: feedforge-adoption
description: Guides an agent integrating FeedForge into an external CMake C++20 application. Use when choosing a canonical generated target or custom pipeline generation, wiring find_package or FetchContent, implementing typed noexcept sinks, replaying Nasdaq BinaryFILE input, handling outcomes and lifetimes, or verifying an installed consumer without dependency or flag leakage.
---

# FeedForge adoption

Use this workflow for consumer integration. Keep application changes target-scoped and verify public names against this repository rather than inventing wrappers.

## 1. Choose the integration path

- Use `FeedForge::generated::itch50_order_events` when the eight committed order-event projections are sufficient.
- Use `FeedForge::generated::itch50_all` only when the consumer needs every committed ITCH 5.0 event.
- Use `feedforge_generate()` when the selected messages, projected fields, namespace, or unknown-message policy differs.
- Do not regenerate a canonical header in a consumer. Canonical targets use committed headers and need only a C++20 toolchain.
- Custom generation needs `FeedForge::compiler` and a C++23 host toolchain at build time. The generated target and consuming code remain C++20.

Read [reference.md](reference.md) for exact targets, headers, CMake acquisition patterns, statuses, and value types.

## 2. Integrate through CMake targets

Prefer a config package:

```cmake
find_package(FeedForge CONFIG REQUIRED)
target_compile_features(my_decoder PRIVATE cxx_std_20)
target_link_libraries(
  my_decoder
  PRIVATE FeedForge::generated::itch50_order_events
)
```

For custom generation, require the compiler explicitly, call
`feedforge_generate()`, and link the generated target. Omit `OUTPUT` for the
default `<feedforge/generated/NAME.hpp>` layout. If a custom location is
required, make it end in `feedforge/generated/NAME.hpp` to preserve that include
spelling.

Do not add FeedForge include directories manually, copy generated headers out of the target, modify global compiler flags, or link toml++ in the consumer. Do not make warning, sanitizer, exception, RTTI, or optimization flags transitive.

## 3. Implement the sink contract

Write one explicit overload for every selected generated event:

```cpp
feedforge::flow operator()(events::event_type const& event) noexcept;
```

Every overload must be unambiguous, return exactly `feedforge::flow`, and be `noexcept`. Add:

```cpp
static_assert(events::sink_for_all_selected_events<my_sink>);
```

Do not rely on a catch-all function template for production adoption: it can satisfy the concept while hiding an omitted event-specific handler. See [examples.md](examples.md) for all eight typed order-event overloads.

## 4. Respect ownership and strong types

- The input span must remain valid for `decode_one()` or the whole replay call.
- An event reference is valid only during its sink invocation. Copy the event to retain it; generated events own their projected values.
- Preserve wrappers instead of flattening them: semantic integers and timestamps use `.value`, decimals use `.raw` plus the static `scale`, and fixed ASCII uses `.raw` or `.trimmed()`.
- Keep file I/O, mapping, allocation, logging, and blocking outside FeedForge replay unless the sink intentionally performs them.

## 5. Replay and classify the result

Load or map a complete BinaryFILE first, then pass a
`std::span<const std::byte>` to the generated namespace's
`replay_binary_file()`. For incrementally delivered input, construct
`chunked_replayer<Sink>` with caller-owned scratch, push chunks in order, and
call `finish()` only when no more bytes will arrive. The scratch and sink must
outlive the adapter; chunks must not overlap scratch. Use 65,535 bytes of
scratch for full BinaryFILE payload-length equivalence or treat a smaller span
as an explicit application bound.

- `complete` means a zero-length end marker was consumed with no trailing byte.
- `incomplete` means clean exhaustion at a frame boundary without that marker.
- `stopped` is cooperative termination, not an error.
- `framing_error` uses `framing_error` and `error_offset`.
- `decode_error` uses `decode_error`, including message type and expected/actual size.

For direct `decode_one()`, use `is_error()` and `is_terminal()` rather than treating every non-`emitted` status as failure. Known-unselected and policy-approved unknown skips are successful outcomes.

## 6. Verify the boundary

From a separate consumer build directory:

1. Configure against the install prefix or FetchContent checkout.
2. Build the consumer as C++20.
3. Run a valid empty complete BinaryFILE (`00 00`) and at least one selected fixture.
4. Exercise incomplete, framing-error, decode-error, and sink-stop handling.
5. If input is chunked, split prefixes and payloads and verify the terminal
   result matches one-shot replay.
6. For a canonical package, repeat with `FEEDFORGE_BUILD_COMPILER=OFF`.
7. Inspect exported interface properties if packaging changes: canonical targets must not export project warning/link flags or third-party libraries.

Use the command checklist in [reference.md](reference.md).

## Report integration defects

Do not conceal a package or helper mismatch with manual include paths or copied
files. `OUTPUT` must be a literal build-tree path named `NAME.hpp`. A suffix of
`feedforge/generated/NAME.hpp` preserves the canonical include; other matching
filenames retain the legacy flat `<NAME.hpp>` include. Treat a target that
exposes any other include root as a defect.
