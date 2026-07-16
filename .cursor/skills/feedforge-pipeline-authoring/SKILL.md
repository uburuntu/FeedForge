---
name: feedforge-pipeline-authoring
description: Guides an agent authoring and validating schema-bound FeedForge pipeline TOML. Use when selecting message projections, ordering fields, choosing unknown-message policy, running feedforgec validate or compile, wiring feedforge_generate, interpreting stable diagnostics, checking deterministic generated output, or rejecting unsupported computed fields and embedded C++.
---

# FeedForge pipeline authoring

Apply this workflow to pipeline TOML bound to an existing validated schema. A pipeline selects and names data; it never changes wire layout or defines decoding logic.

## 1. Inspect the schema first

Before writing a projection:

1. Read the schema's exact `name`.
2. Locate each desired message by one-byte `type`.
3. Record projectable field names, offsets, logical types, and wire order.
4. Exclude the discriminator and every `reserved` field.
5. Decide whether unknown discriminators are errors or successful skips.

Never guess a field spelling or C++ type from protocol prose. Use the checked schema.

## 2. Write the closed pipeline grammar

Every pipeline requires:

```toml
format_version = 1
name = "pipeline_name"
namespace = "project::generated::pipeline_name"
schema = "exact_schema_name"
profile = "portable_checked"
unknown_messages = "error"
unselected_messages = "skip"
```

Add at least one `[[emit]]`, each with exactly `source`, `event`, and `fields`.

- `source` is an exact one-byte schema discriminator.
- `event` is a unique lowercase snake-case C++-safe identifier.
- Use explicit fields when the consumer needs a stable intentional projection.
- Use exactly `fields = ["*"]` only when every projectable field is intended.
- Preserve intentional explicit field order: it defines generated member order and is part of the pipeline fingerprint.
- Keep schema field names and strong logical types; pipelines cannot rename or coerce fields.

See [reference.md](reference.md) for the full accepted grammar, type mapping, policies, and diagnostic families. See [examples.md](examples.md) for a complete custom pipeline and commands.

## 3. Reject unsupported designs

Do not put any of these in pipeline TOML or generated-code glue:

- C++ snippets, includes, declarations, or callbacks;
- computed, transformed, conditional, optional, or renamed fields;
- changed offsets, widths, message sizes, endianness, or allowed values;
- wildcard mixed with explicit fields;
- multiple emits for one source discriminator;
- an unselected-message policy other than `"skip"`;
- a profile other than `"portable_checked"`;
- silent/ignore policy spellings.

If the requested behavior needs one of these, report it as outside pipeline format version 1. Perform application-level computation in a typed sink, or propose a separately reviewed format/compiler change.

## 4. Validate before generation

Run schema-only validation first, then schema-plus-pipeline validation:

```sh
build/feedforge-tools/src/feedforgec/feedforgec validate \
  --schema schemas/nasdaq/totalview_itch_5_0.toml

build/feedforge-tools/src/feedforgec/feedforgec validate \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline path/to/pipeline.toml
```

Replace `build/feedforge-tools` with the actual compiler-enabled FeedForge build directory. A successful validation is silent and exits zero.

Read a failure as:

```text
CODE path:line:column: object.path: message
hint: optional remediation
```

Fix the object named after the location. Do not scrape message prose when a stable code is available. `FFTOML`, `FFSCHEMA`, `FFPIPE`, `FFCLI`, `FFIO`, and `FFEMIT` identify the failing layer.

## 5. Compile and inspect

Create output directories before invoking the CLI, then compile:

```sh
cmake -E make_directory build/generated
build/feedforge-tools/src/feedforgec/feedforgec compile \
  --schema schemas/nasdaq/totalview_itch_5_0.toml \
  --pipeline path/to/pipeline.toml \
  --output build/generated/pipeline.hpp \
  --dump-ir build/generated/pipeline.ffir.json
```

Inspect the generated header for the configured namespace, event names, exact member order/types, sink concept, metadata fingerprints, decoder, and replay adapter. Treat it as generated output; do not hand-edit it.

## 6. Wire repeatable CMake generation

Use a compiler-enabled package or FetchContent dependency, then:

```cmake
feedforge_generate(
  NAME pipeline_name
  SCHEMA nasdaq_totalview_itch_5_0
  PIPELINE "${CMAKE_CURRENT_SOURCE_DIR}/pipeline.toml"
)
target_link_libraries(
  consumer
  PRIVATE FeedForge::generated::pipeline_name
)
```

The default header is `<feedforge/generated/pipeline_name.hpp>`. The generated C++ namespace still comes from TOML. Keep `NAME`, pipeline `name`, and namespace leaf aligned for clarity.

Omit `OUTPUT` unless a custom build-tree location is needed. To retain the
default include, make an explicit path end in
`feedforge/generated/pipeline_name.hpp`; see [reference.md](reference.md).

## 7. Prove determinism and compileability

Compile the same semantic inputs into two existing output directories and compare the header bytes with `cmake -E compare_files`. If only comments, TOML key order, or emit-table order changed, output must remain byte-identical. A change to explicit field order is semantic and should change the pipeline fingerprint/output.

Finally build a C++20 consumer that includes the generated header, defines every explicit typed `noexcept` sink overload, asserts `sink_for_all_selected_events`, and exercises one selected message plus one known-unselected message.
