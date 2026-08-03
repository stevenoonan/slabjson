# Contributing to SlabJson

Thank you for improving SlabJson. Changes should preserve its fixed-capacity,
allocation-free core and its C++20 portability.

## Development setup

Configure, build, and test a Debug checkout with:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Before opening a pull request, also run the Release and no-exceptions presets:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release

cmake --preset no-exceptions
cmake --build --preset no-exceptions
ctest --preset no-exceptions
```

The sanitizer preset requires a compiler that supports AddressSanitizer and
UndefinedBehaviorSanitizer.

## Pull requests

- Keep each change focused and include tests for observable behavior.
- Do not introduce heap allocation, exceptions, or RTTI into the core library.
- Preserve transactional failure behavior where the API promises it.
- Update `README.md` and `CHANGELOG.md` for user-visible changes.
- Treat public headers and the `slabjson::slabjson` CMake target as a deliberate
  pre-1.0 API. Document breaking changes and reserve them for a new 0.x minor
  release.
- Ensure all CI checks pass before requesting review.

By contributing, you agree that your contribution is licensed under the MIT
License in this repository.
