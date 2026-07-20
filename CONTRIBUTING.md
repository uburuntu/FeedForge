# Contributing to FeedForge

FeedForge is a narrow, correctness-first compiler and runtime. Contributions
should preserve its explicit protocol, validation, determinism, and hot-path
contracts rather than broaden scope incidentally.

## Before opening a change

- Read the [architecture](docs/architecture.md), format contracts, generated
  API, and [testing guide](docs/testing.md) before changing behavior.
- Check existing issues and open a focused issue for behavioral or scope
  changes before implementation.
- Do not include exchange data, credentials, proprietary captures, or private
  benchmark artifacts.
- Keep runtime/generated public targets free of third-party dependencies.

## Development setup

Required tools are CMake 3.25 or newer, Ninja, Git, and a suitable C++ compiler.
Run:

```sh
make doctor
make quick
make dev
```

Before submitting a substantial change, run the relevant focused targets and
the portable matrix:

```sh
make verify
```

Hosted Linux, macOS, Windows, sanitizer, and libFuzzer jobs remain authoritative
for platform-specific release evidence.

## Generated files

Do not hand-edit files under `generated/`. Change the schema, pipeline, runtime,
or emitter, then use the guarded regeneration flow:

```sh
make generated-refresh CONFIRM=regenerate
make generated-check
```

Review generated changes as carefully as handwritten source. Deterministic
output, provenance, and compatibility metadata are public contracts.

## Protocol fixtures

Schema and fixture changes require authoritative source citations and an
independent review. Follow [docs/testing.md](docs/testing.md) and update the
source lock and schema audit where applicable. Generated fixtures are not a
substitute for independently reviewed wire bytes.

## Performance work

Correctness comes first. Performance changes must follow
[docs/benchmarking.md](docs/benchmarking.md), preserve all semantics, and avoid
public claims without the required holdout confirmation. Benchmark artifacts
belong under ignored build/output directories unless a review explicitly
requires a durable external artifact.

## Pull requests

Keep each pull request focused. Explain the contract affected, tests run, and
any generated output or documentation changes. Public contract changes need a
durable architecture decision or documentation update before implementation.
Warnings are treated as errors in CI. New behavior needs focused tests at the
lowest useful layer plus broader integration coverage when it crosses component
boundaries.
