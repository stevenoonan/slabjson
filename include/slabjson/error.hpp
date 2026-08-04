#pragma once

#include <cstddef>
#include <cstdint>

namespace slabjson {

enum class ErrorCode : std::uint8_t {
    Ok,
    InvalidArgument,
    InvalidHandle,
    TypeMismatch,
    NotFound,
    AlreadyAttached,
    CrossSlab,
    CycleDetected,
    InvalidUtf8,
    NonFiniteNumber,

    OutOfMemory,
    StringCapacityExceeded,
    ParserDepthExceeded,
    OutputCapacityExceeded,

    ParseUnexpectedEnd,
    ParseUnexpectedToken,
    ParseInvalidString,
    ParseInvalidNumber,
    ParseInvalidEscape,
    ParseInvalidUnicodeEscape,
    ParseTrailingCharacters,

    InternalError
};

struct Error {
    ErrorCode code{ErrorCode::Ok};
    std::size_t offset{0};
};

} // namespace slabjson
