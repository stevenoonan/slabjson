# Changelog

All notable changes to this project are documented in this file. The format is
based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this
project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.9.0] - 2026-08-03

### Added

- Allocation-free C++20 JSON DOM backed by caller-provided slab storage.
- Exact signed integer, unsigned integer, and floating-point storage.
- Transactional parsing, duplication, and serialization APIs.
- Compact, pretty, and single-pass partial serialization.
- UTF-8 validation and JSON string escaping.
- Native object and array APIs plus cJSON-style migration helpers.
- Static CMake package support through `slabjson::slabjson`.

[Unreleased]: https://github.com/stevenoonan/slabjson/compare/v0.9.0...HEAD
[0.9.0]: https://github.com/stevenoonan/slabjson/releases/tag/v0.9.0
