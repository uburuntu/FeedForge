# Security model

FeedForge separates an offline host compiler from a header-only runtime. It is
experimental, is not exchange-certified, and is not production trading
infrastructure.

## Inputs and trust boundaries

- Schema and pipeline TOML are untrusted compiler inputs. The compiler rejects
  unknown keys, invalid layouts, unsafe identifiers, aliases, and resources
  beyond the published [compiler limits](compiler-limits.md).
- Generated C++ is build input. Review and compile it with the same controls as
  other generated source; FeedForge does not execute it during generation.
- Compiler output uses an exclusively created sibling temporary file followed
  by platform-native atomic replacement. The caller still controls and must
  trust the destination directory and storage stack.
- BinaryFILE bytes are untrusted runtime input. Framing and exact message size
  are checked before projected loads. Runtime code performs no file or network
  I/O and owns no input memory.
- Sinks are application code. Their allocation, blocking, persistence, and
  side effects are outside FeedForge's guarantees.

One-shot replay is bounded by the caller's input span. Chunked replay retains
only framing state and copies split payloads into caller-owned scratch; payloads
larger than that span fail with `insufficient_scratch`. Replay counters and
absolute offsets use 64-bit unsigned values, and offset exhaustion is a
terminal framing error.

## Explicit non-goals

FeedForge provides no networking, capture authentication, sequence recovery,
order-book state, strategy controls, exchange certification, process sandbox,
or protection from a malicious compiler/toolchain. It does not validate that
a data source is licensed or authoritative.

Report vulnerabilities through the private process in [SECURITY.md](../SECURITY.md).
