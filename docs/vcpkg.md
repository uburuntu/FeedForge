# Local vcpkg overlay

FeedForge owns a local overlay port under
`packaging/vcpkg/ports/feedforge`. The port builds from the containing source
checkout; it does not download FeedForge or stand in for a registry release.
Pin the FeedForge checkout separately and pass its overlay directory explicitly.
Because the input is a mutable checkout rather than a source archive with a
content hash, use this overlay with `VCPKG_BINARY_SOURCES=clear` so a package
built from another FeedForge revision cannot be reused.

The default port installs the C++20 runtime, canonical generated headers,
schemas, documentation, and CMake package without building `feedforgec` or
toml++. The optional `compiler` feature builds the native C++23 host compiler
and depends on vcpkg's `tomlplusplus` port. The feature is native-only; it is not
a cross-compiled target tool.

## Pinned consumer manifest

Use the repository-tested builtin baseline:

```json
{
  "name": "feedforge-consumer",
  "version-string": "0",
  "builtin-baseline": "3ddaad9be959816602453ecb05533f8732464ef4",
  "dependencies": [
    {
      "name": "feedforge",
      "default-features": false
    }
  ]
}
```

Install from the FeedForge checkout with binary package reuse disabled:

```sh
VCPKG_BINARY_SOURCES=clear "$VCPKG_ROOT/vcpkg" install \
  --overlay-ports="$FEEDFORGE_SOURCE/packaging/vcpkg/ports"
```

Pass the same overlay to CMake through `VCPKG_OVERLAY_PORTS`, then consume the
normal package targets:

```sh
VCPKG_BINARY_SOURCES=clear cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_OVERLAY_PORTS="$FEEDFORGE_SOURCE/packaging/vcpkg/ports"
cmake --build build
```

```cmake
find_package(FeedForge CONFIG REQUIRED)
target_link_libraries(
  consumer
  PRIVATE FeedForge::generated::itch50_order_events
)
```

## Compiler feature

Request the compiler explicitly; it is never a default feature:

```json
{
  "name": "feedforge-compiler-consumer",
  "version-string": "0",
  "builtin-baseline": "3ddaad9be959816602453ecb05533f8732464ef4",
  "dependencies": [
    {
      "name": "feedforge",
      "default-features": false,
      "features": ["compiler"]
    }
  ]
}
```

The installed package then provides `FeedForge::compiler` and
`feedforge_generate()` in addition to the runtime and canonical targets.

## Repository validation

Bootstrap vcpkg at the pinned baseline, then run both clean consumer paths with
binary caching disabled:

```sh
git -C "$VCPKG_ROOT" checkout 3ddaad9be959816602453ecb05533f8732464ef4
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
cmake -E env VCPKG_BINARY_SOURCES=clear \
  cmake \
    -DSOURCE_DIR="$FEEDFORGE_SOURCE" \
    -DVCPKG_ROOT="$VCPKG_ROOT" \
    -DBUILD_ROOT="$FEEDFORGE_SOURCE/build/vcpkg-overlay" \
    -P "$FEEDFORGE_SOURCE/packaging/vcpkg/tests/run_overlay.cmake"
```

The runtime scenario asserts that neither the compiler nor toml++ leaks into
the default dependency graph. The compiler scenario generates, builds, and runs
a custom C++20 pipeline consumer. Normal FeedForge CTest runs also verify that
the port version, feature dependency, baseline, port options, and this guide
remain synchronized without invoking vcpkg or the network.
