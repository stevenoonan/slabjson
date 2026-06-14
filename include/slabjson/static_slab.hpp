#pragma once

#include <array>
#include <cstddef>
#include <span>

#include <slabjson/slab.hpp>

namespace slabjson {

template <std::size_t Bytes>
class StaticSlab final : public Slab {
public:
    StaticSlab() noexcept
    {
        initialize(std::span<std::byte>{storage_});
    }

private:
    alignas(std::max_align_t) std::array<std::byte, Bytes> storage_{};
};

} // namespace slabjson
