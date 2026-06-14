#include <array>
#include <cstddef>
#include <iostream>
#include <span>

#include <slabjson/slabjson.hpp>

namespace {

int failures = 0;

void check(bool condition, const char* expression, int line)
{
    if (!condition) {
        std::cerr << "line " << line << ": check failed: " << expression << '\n';
        ++failures;
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

} // namespace

int main()
{
    {
        slabjson::Result<void> success;
        CHECK(success);
        success.value();

        slabjson::Result<void> failure{
            slabjson::Error{slabjson::ErrorCode::InvalidArgument, 7},
        };
        CHECK(!failure);
        CHECK(failure.error().code == slabjson::ErrorCode::InvalidArgument);
        CHECK(failure.error().offset == 7);
    }

    {
        slabjson::StaticSlab<1024> slab;
        CHECK(slab.valid());
        CHECK(slab.capacity_bytes() == 1024);
        CHECK(slab.used_bytes() == 0);
        CHECK(slab.remaining_bytes() == slab.capacity_bytes());

        auto value = slab.make_null();
        CHECK(value);
        CHECK(slab.used_bytes() > 0);
        CHECK(slab.remaining_bytes() + slab.used_bytes() == slab.capacity_bytes());

        slab.reset();
        CHECK(slab.used_bytes() == 0);
        CHECK(slab.remaining_bytes() == slab.capacity_bytes());
    }

    {
        alignas(std::max_align_t) std::array<std::byte, 257> storage{};
        slabjson::Slab slab{
            std::span<std::byte>{storage}.subspan(1),
        };
        CHECK(slab.valid());
        CHECK(slab.capacity_bytes() < 256);

        auto value = slab.make_number(12.5);
        CHECK(value);
        CHECK(value.value().as_number() == 12.5);
    }

    {
        std::array<std::byte, 1> storage{};
        slabjson::Slab slab{storage};
        CHECK(!slab.valid());

        auto value = slab.make_null();
        CHECK(!value);
        CHECK(value.error().code == slabjson::ErrorCode::InvalidArgument);
    }

    {
        slabjson::StaticSlab<128> slab;
        std::size_t allocated = 0;
        while (slab.make_null()) {
            ++allocated;
        }
        CHECK(allocated > 0);

        auto exhausted = slab.make_null();
        CHECK(!exhausted);
        CHECK(exhausted.error().code == slabjson::ErrorCode::OutOfMemory);
        CHECK(slab.remaining_bytes() < slab.capacity_bytes());
    }

    return failures == 0 ? 0 : 1;
}
