# Changelog

All notable changes to this project are documented in this file. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.1] - Pending

### Added

- Optional `COMPACT32` and `INTEGER32` numeric modes with 16-byte nodes, native
  numeric aliases (`int_t`, `uint_t`, optional `float_t`) and accessors, and
  compile-time rejection of wider construction.
- Independent parser, serializer, pretty-output, and cJSON compatibility
  switches, installed build configuration, and size-oriented MCU presets.
- Repeatable Cortex-M4 footprint reports with linked-size and dependency
  checks, DOM consumption, buffer budgets, and compiler stack reports.
- Sticky first-error status for object and array additions, with shared slab
  status checks and explicit error clearing.
- Standalone JSONTestSuite conformance, allocation guard tests, and bounded
  sanitizer fuzzing in CI, with opt-in conformance and fuzz build options.

### Changed

- Parsing uses a bounded array of eight-byte container frames instead of
  recursion, with a configurable compiled maximum depth.
- Opt-in size optimization shares serializer traversal across measurement and
  output while retaining transactional and partial serialization contracts.
- Addition results may be intentionally deferred and checked once through the
  containing object, array, or slab status.
- Parsing and programmatic string construction share UTF-8 sequence validation
  while preserving their error codes and byte offsets.
- Object and array construction share status, allocation, attachment, and
  rollback handling through a private helper.
- Object and array `empty()` checks inspect the first child in constant time.
- Release tests derive versions from CMake project metadata and use isolated
  fixtures with positive controls and specific rejection-message checks.

### Fixed

- Serializer string slices avoid a throwing standard-library bounds path that
  could retain exception and allocation support in embedded executables.
- cJSON printing checks the terminating NUL for slab overlap before writing,
  preventing corruption when the output buffer immediately precedes the slab.
- Floating-point negative zero serializes as `-0.0`, preserving its sign and
  floating-point representation when parsed again.
- Memory reports fail when corpus operations fail and measure canonical
  capacity checks instead of reporting hardcoded results.
- Installed-consumer tests honor configured library, header, and documentation
  directories and verify archives through the imported CMake target.
- Invalid-UTF-8 object-key coverage clears the earlier sticky value error,
  verifies rollback, and checks that the rejected child can be attached again.

## [0.9.0] - 2026-08-03

### Added

- Allocation-free C++20 JSON DOM backed by caller-provided slab storage.
- Exact signed integer, unsigned integer, and floating-point storage.
- Transactional parsing, duplication, and serialization APIs.
- Compact, pretty, and single-pass partial serialization.
- UTF-8 validation and JSON string escaping.
- Native object and array APIs plus cJSON-style migration helpers.
- Static CMake package support through `slabjson::slabjson`.

[0.9.1]: https://github.com/stevenoonan/slabjson/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/stevenoonan/slabjson/releases/tag/v0.9.0
