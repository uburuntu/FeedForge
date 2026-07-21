# Compiler limits

Schema and pipeline files are untrusted compiler inputs. `feedforgec` applies
the following deterministic limits before generation; exceeding one returns
`FFLIMIT001` and exit status 2.

| Resource | Limit |
|---|---:|
| Schema or pipeline source | 1,048,576 bytes each |
| TOML nesting | 32 values |
| TOML nodes | 32,768 per document |
| Identifier | 128 bytes |
| C++ namespace | 16 components and 512 bytes |
| Documentation or provenance string | 4,096 bytes |
| User-defined schema types | 256 |
| Messages | 94 |
| Pipeline projections | 94 |
| Fields in one message or projection | 1,024 |
| Total schema or projected fields | 8,192 |
| Allowed values on one field | 256 values and 65,535 total bytes |
| Generated C++ or canonical FFIR JSON | 16,777,216 bytes |

Limits count bytes, not Unicode code points. FeedForge identifiers are ASCII by
grammar. The published Nasdaq schema and canonical pipelines are substantially
below every limit.

The limits bound compiler work; they do not turn compilation into a sandbox.
Run the host compiler with ordinary least privilege and review generated source
before using inputs from an untrusted party. Runtime bounds are described in
[Security model](security-model.md).
