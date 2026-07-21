# Compatibility

FeedForge versions the installable package, generated headers, schema and
pipeline formats, FFIR, implementation profile, and runtime source API
separately. They are related, but none is a substitute for another.

## Package versions

FeedForge follows semantic versioning. Before 1.0, installed CMake packages use
same-minor compatibility: a consumer requesting 0.3 will not silently accept
0.4. Source compatibility may change between pre-1.0 minor releases and is
described in each release's notes.

FeedForge does not promise a stable binary ABI. Rebuild consumers and generated
code when changing package versions.

## Runtime and generated headers

The public runtime exposes two constants:

```cpp
feedforge::runtime_api_epoch
feedforge::runtime_api_revision
```

Every generated header records `required_runtime_api_epoch` and
`minimum_runtime_api_revision`. Compilation succeeds only when the epochs are
equal and the installed runtime revision is at least the generated header's
minimum. An epoch change is an intentional source-compatibility break. A
revision increase is additive within that epoch.

This check prevents known-incompatible runtime/header combinations; it is not
an ABI guarantee. Regenerating custom headers with the installed compiler is
the preferred upgrade path.

## Other versioned contracts

- Schema and pipeline `format_version` values version their TOML grammars.
- FFIR `format_version` versions the resolved model. Canonical FFIR JSON is a
  deterministic diagnostic artifact, not a long-term interchange promise.
- `portable_checked.v1` identifies the emitted implementation profile and its
  observable decode semantics.
- Schema and pipeline fingerprints identify resolved input semantics, not file
  spelling or paths.

Replay counters and offsets are `std::uint64_t`. Event field types remain
determined by the schema-to-C++ mapping in [Generated C++ API](generated-api.md).
