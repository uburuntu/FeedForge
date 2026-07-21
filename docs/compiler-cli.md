# Compiler CLI

`feedforgec` is a C++23 host tool. Successful commands are silent and return
zero.

```text
feedforgec validate --schema <file> [--pipeline <file>]
feedforgec compile --schema <file> --pipeline <file> --output <file>
feedforgec dump-ir --schema <file> --pipeline <file> --output <file>
feedforgec --version
feedforgec --help
```

`validate` parses and validates without writing output. `compile` writes one
deterministic C++20 header. `dump-ir` writes one canonical FFIR JSON document
for inspection and reproducibility checks. FFIR JSON is not a stable public
interchange format.

The output must not name either input. The parent directory must already
exist. A failed validation, emission, or write leaves an existing destination
unchanged and removes compiler-owned temporary files.

## Exit status

| Status | Meaning |
|---:|---|
| 0 | Success |
| 2 | CLI, TOML, semantic, limit, or emission error |
| 3 | Input or output I/O error |
| 4 | Unexpected internal compiler failure |

Diagnostics begin with a stable code such as `FFSCHEMA002`, `FFLIMIT001`, or
`FFIO001`, followed by normalized source and object paths. Compiler limits are
documented in [Compiler limits](compiler-limits.md).
