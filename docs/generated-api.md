# Generated C++ API

> **Availability:** This document specifies the normative v0.1 generated API
> from SPEC Sections 17–19. Runtime names were checked against the public
> headers, but this contract alone does not establish that a particular
> generated pipeline is available. Availability requires its generated header
> and corresponding conformance/release evidence.

FeedForge is experimental, is not exchange-certified, and is not production
trading infrastructure. v0.1 offers source-level contracts, not a stable binary
ABI.

## Header and namespace

Each pipeline is intended to generate one include-order-independent C++20
header in the pipeline's configured namespace. Canonical artifacts are:

- `<feedforge/generated/nasdaq/itch50_all.hpp>` in
  `feedforge::generated::nasdaq::itch50_all`; and
- `<feedforge/generated/nasdaq/itch50_order_events.hpp>` in
  `feedforge::generated::nasdaq::itch50_order_events`.

Generated headers depend on the public FeedForge runtime headers and C++20
standard library only. Each header records the required runtime source-API
version and statically compares it with `feedforge::runtime_api_version`.

## Event types

Every pipeline `[[emit]]` produces one aggregate event struct named by its
`event` value. An event:

- contains exactly the projected fields, in pipeline declaration order;
- owns every field value and retains no pointer, reference, span, or view into
  input;
- is default constructible, standard-layout, and trivially copyable;
- has no `string`, `vector`, `optional`, `variant`, or other heap-owning member;
- defines equality for tests; and
- exposes static `std::byte source_discriminator` and
  `std::string_view event_name` constants.

The schema-to-C++ mapping is exact:

- `raw_unsigned` widths 1, 2, 4, 6, and 8 map to `std::uint8_t`,
  `std::uint16_t`, `std::uint32_t`, `std::uint64_t`, and `std::uint64_t`;
- fixed-width `ascii` and built-in `alpha` map to `feedforge::ascii<N>`;
- `timestamp_ns` maps to `feedforge::timestamp_ns`;
- 4- or 8-byte `decimal` at scale `S` maps to
  `feedforge::decimal<std::uint32_t, S>` or
  `feedforge::decimal<std::uint64_t, S>`;
- semantic integers map to `feedforge::stock_locate`,
  `tracking_number`, `order_reference_number`, `match_number`, or
  `share_count`; and
- discriminator and `reserved` fields produce no event member.

Prices remain scaled integers, timestamps remain unsigned nanoseconds since
midnight, and ASCII values preserve all bytes. `ascii<N>::trimmed()` removes
only trailing ASCII spaces and returns a non-owning `std::string_view`.

## Sink contract and lifetime

A sink provides one unambiguous overload for every selected event:

```cpp
feedforge::flow operator()(Event const& event) noexcept;
```

The generated decoder rejects a missing, ambiguous, throwing, or wrong-result
overload at compile time. Delivery is statically bound; there is no virtual
sink and no `std::function`. `feedforge::flow` has exactly two values:
`continue_` and `stop`.

The event reference is valid only for the sink invocation. A sink needing the
event afterward must copy it; retaining the reference is invalid. Because the
event owns its fields, such a copy has no dependency on the input span. Sink
allocation, blocking, and storage behavior remain the sink author's
responsibility.

## Decoder

The generated decoder has this semantic public shape:

```cpp
template <feedforge::decoder_implementation Implementation>
class basic_decoder {
 public:
  template <class Sink>
    requires sink_for_all_selected_events<Sink>
  [[nodiscard]] feedforge::decode_outcome
  decode_one(std::span<const std::byte> payload, Sink& sink) const noexcept;
};

using decoder = basic_decoder<feedforge::profile::portable_checked>;
```

`sink_for_all_selected_events` denotes the generated compile-time requirement
over all selected event types; an implementation may keep that helper name
internal. `decoder` is the public alias selected by the pipeline's cohesive
profile. The v0.1 profile uses variant ID `portable_checked.v1`.

`decode_one` performs these operations in order:

1. reject an empty payload without reading it;
2. read the one-byte discriminator and determine whether it is known;
3. obtain the declared size for every known type;
4. compare exact size before any projected-field load;
5. return a skip for a valid known but unselected message;
6. load only selected fields for a selected message;
7. construct one owning event;
8. invoke the sink exactly once; and
9. return stop if requested, without classifying stop as an error.

The caller must keep `payload` valid for the duration of the call. Neither the
decoder nor event retains it. FeedForge-owned decode work is required to be
`noexcept` and to allocate no dynamic memory after caller setup.

### Decode outcomes

The runtime defines these exact statuses:

```cpp
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
```

For every status:

- `actual_size` is exactly `payload.size()` and is never narrowed;
- `message_type` is `payload[0]` for nonempty input and `std::byte{0}` for
  empty input; and
- `expected_size` is the schema size for a known type, otherwise zero.

Status meanings are:

- `emitted`: a selected valid event was delivered and the sink returned
  `continue_`;
- `stopped`: a selected valid event was delivered and the sink returned
  `stop`;
- `known_unselected_skipped`: a known unselected message passed exact-size
  validation and no sink was called;
- `unknown_skipped`: the discriminator was unknown and pipeline policy was
  `"skip"`;
- `empty_payload`: no discriminator byte was available;
- `unknown_message_type`: the discriminator was unknown and policy was
  `"error"`; and
- `invalid_message_size`: a known message's actual size differed from its
  exact declared size.

`empty_payload`, `unknown_message_type`, and `invalid_message_size` are errors.
`stopped` is terminal but is not an error. `is_terminal()` is true for stop and
all three errors; `is_error()` is true only for those errors. Unknown
alpha/code field values are preserved and do not create a semantic error in
v0.1.

## Strict BinaryFILE replay

Each generated namespace supplies a thin adapter with this semantic shape:

```cpp
template <class Sink>
  requires sink_for_all_selected_events<Sink>
[[nodiscard]] feedforge::replay_summary
replay_binary_file(std::span<const std::byte> input, Sink& sink) noexcept;
```

It combines `feedforge::binary_file_cursor` with that namespace's `decoder`.
The caller must keep `input` valid through the call. The adapter does no I/O;
loading or mapping a file is caller setup.

The runtime result is:

```cpp
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
```

Replay obtains one complete frame, decodes its payload, and only then requests
the next frame. It stops at the first framing error, decode error, or sink stop.
Once stopped, it does not inspect later bytes or search for an end marker.

The terminal states are distinct:

- `complete`: a zero-length BinaryFILE end marker was reached with no trailing
  byte;
- `incomplete`: input ended cleanly at a frame boundary without an end marker;
- `stopped`: a sink requested cooperative stop;
- `framing_error`: a one-byte length prefix, truncated payload, or byte after
  an end marker was encountered; and
- `decode_error`: a complete nonzero frame had an
  unknown-under-error-policy or wrong-sized payload.

`empty_payload` is possible when calling `decode_one` directly. It is not a
strict replay frame state because a zero BinaryFILE payload length is the end
marker and is never passed to the decoder.

Strict replay rejects every byte after a zero marker. It never guesses
complete/incomplete when stop or error occurred first.

### Counters and offsets

- `frames_seen` increments when a complete nonzero frame is obtained, before
  decoding. An unknown, wrong-sized, or sink-stopped frame therefore counts;
  a malformed frame record and zero marker do not.
- `events_emitted` includes the event whose sink requested stop.
- Skip counters increment only for their corresponding successful decode skip.
- `bytes_consumed` includes each accepted frame prefix and payload. It includes
  the zero marker on `complete`, the stopped/erroring complete frame on
  `stopped` or `decode_error`, and stops at a malformed record's prefix on
  `framing_error`.
- A decode `error_offset` identifies the frame payload's first byte. A framing
  offset identifies the offending length prefix or first trailing byte.

`framing_error` is meaningful only for `framing_error`; `decode_error` is
meaningful only for `decode_error`; otherwise error-specific members remain
defaulted. No result owns diagnostic text.

## Generated checks and provenance

Generated headers statically check known message sizes, projected field bounds,
supported widths/scales, event traits, discriminator/event-name uniqueness,
implementation concept satisfaction, and generated/runtime source-API
compatibility.

Each header also records FeedForge/compiler version, FFIR version, schema and
pipeline semantic fingerprints, and profile/variant ID. It must not contain
generation time, hostname, username, random IDs, or absolute source paths. See
[Architecture](architecture.md) and [Testing](testing.md) for the corresponding
determinism and conformance requirements.
