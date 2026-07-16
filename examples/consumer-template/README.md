# FeedForge external consumer template

Copy this directory outside the FeedForge checkout. The default path consumes
the committed order-events target as C++20 and does not require
`FeedForge::compiler`, C++23, or toml++.
It is a standalone project and is not added to FeedForge's normal example or
benchmark targets.

```sh
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/feedforge-prefix
cmake --build build
cmake --build build --target run-feedforge-consumer
```

The executable replays one framed Add Order followed by a complete marker. Its
sink has an explicit `noexcept` overload for every selected event.

To demonstrate custom generation, install FeedForge with
`FEEDFORGE_BUILD_COMPILER=ON` and configure a separate build:

```sh
cmake -S . -B build/custom \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/compiler-enabled-feedforge-prefix \
  -DFEEDFORGE_TEMPLATE_CUSTOM_PIPELINE=ON
cmake --build build/custom
cmake --build build/custom --target run-feedforge-consumer
```

The custom executable remains C++20. Only the installed host compiler used to
generate its header requires C++23.
