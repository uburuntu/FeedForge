# ADR 0001: toml++ for host TOML parsing

Status: Accepted

FeedForge's C++23 host compiler uses toml++ v3.4.0 (commit
`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`). An exact-version system package
is preferred when available; otherwise CMake fetches that commit only while
building `feedforgec`.

toml++ is MIT licensed. It is a mature, header-only TOML 1.0 parser that
provides duplicate-key rejection and source line/column regions needed for
stable compiler diagnostics. The dependency is private to the compiler
frontend and is not linked or exposed by `FeedForge::runtime` or generated
consumer targets.
