# Changelog

All notable changes to this project are documented in this file. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.9.1] - Pending

### Added

- Sticky first-error status for object and array additions, with shared slab
  status checks and explicit error clearing.

### Changed

- Addition results may be intentionally deferred and checked once through the
  containing object, array, or slab status.

### Fixed

- cJSON printing checks the terminating NUL for slab overlap before writing,
  preventing corruption when the output buffer immediately precedes the slab.

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
