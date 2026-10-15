#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#include <slabjson/cjson_compat.hpp>
#include <slabjson/slabjson.hpp>

namespace {
bool tracking = false;
std::size_t allocation_count = 0;

void record_allocation() noexcept
{
    if (tracking) {
        ++allocation_count;
    }
}

[[noreturn]] void allocation_failed()
{
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    throw std::bad_alloc{};
#else
    std::abort();
#endif
}

bool exercise_core()
{
    slabjson::StaticSlab<8192> source;
    slabjson::StaticSlab<8192> destination;
    auto parsed = slabjson::parse(source,
        R"({"text":"\u00e9\n\uD83D\uDE00","numbers":[-0.0,1.25e-200,18446744073709551615]})");
    if (!parsed) return false;
    const auto used = source.used_bytes();
    if (slabjson::parse(source, R"({"incomplete":[1,})")
        || source.used_bytes() != used) return false;
    auto copy = slabjson::cjson::duplicate(destination, parsed.value());
    if (!copy) return false;
    std::array<char, 512> output{};
    for (bool pretty : {false, true}) {
        auto size = pretty ? slabjson::serialized_size_pretty(copy.value())
                           : slabjson::serialized_size(copy.value());
        auto written = pretty ? slabjson::serialize_pretty(copy.value(), output)
                              : slabjson::serialize(copy.value(), output);
        auto partial = pretty ? slabjson::serialize_pretty_partial(copy.value(), output)
                              : slabjson::serialize_partial(copy.value(), output);
        if (!size || !written || !partial
            || size.value() != written.value() || size.value() != partial.value()) return false;
    }
    if (!slabjson::cjson::print_unformatted(parsed.value(), output)
        || !slabjson::cjson::print_pretty(parsed.value(), output)) return false;
    auto object = source.make_object().value();
    auto array = object.add_array("items");
    if (!array || !array.value().add("utf8: é")
        || !array.value().add(std::numeric_limits<std::int64_t>::min())
        || !array.value().add(std::numeric_limits<std::uint64_t>::max())
        || !array.value().add_null()) return false;
    if (object.add("bad", static_cast<const char*>(nullptr)) || object.status()) return false;
    object.clear_error();
    if (!object.add("valid", true) || !object.remove("valid")) return false;
    slabjson::StaticSlab<24> small;
    auto existing = small.make_bool(true).value();
    if (slabjson::cjson::duplicate(small, parsed.value())
        || existing.as_bool() != true) return false;
    source.reset();
    destination.reset();
    return !parsed.value().valid() && !copy.value().valid();
}
} // namespace

void* operator new(std::size_t size)
{
    record_allocation();
    if (void* pointer = std::malloc(size == 0 ? 1 : size)) return pointer;
    allocation_failed();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

void* operator new(std::size_t size, std::align_val_t alignment)
{
    record_allocation();
    const auto align = static_cast<std::size_t>(alignment);
#if defined(_MSC_VER)
    void* pointer = _aligned_malloc(size == 0 ? 1 : size, align);
#else
    void* pointer = nullptr;
    if (posix_memalign(&pointer, align, size == 0 ? 1 : size) != 0) pointer = nullptr;
#endif
    if (pointer) return pointer;
    allocation_failed();
}
void* operator new[](std::size_t size, std::align_val_t alignment)
{ return ::operator new(size, alignment); }
void operator delete(void* pointer, std::align_val_t) noexcept
{
#if defined(_MSC_VER)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
void operator delete[](void* pointer, std::align_val_t alignment) noexcept
{ ::operator delete(pointer, alignment); }
void operator delete(void* pointer, std::size_t, std::align_val_t alignment) noexcept
{ ::operator delete(pointer, alignment); }
void operator delete[](void* pointer, std::size_t, std::align_val_t alignment) noexcept
{ ::operator delete(pointer, alignment); }

#if defined(SLABJSON_WRAP_ALLOCATIONS)
extern "C" {
void* __real_malloc(std::size_t);
void* __real_calloc(std::size_t, std::size_t);
void* __real_realloc(void*, std::size_t);
void* __wrap_malloc(std::size_t size)
{ record_allocation(); return __real_malloc(size); }
void* __wrap_calloc(std::size_t count, std::size_t size)
{ record_allocation(); return __real_calloc(count, size); }
void* __wrap_realloc(void* pointer, std::size_t size)
{ record_allocation(); return __real_realloc(pointer, size); }
}
#endif

int main()
{
    // Positive controls ensure Release optimizations cannot disable the guard.
    tracking = true;
    void* control = ::operator new(16);
    tracking = false;
    ::operator delete(control);
    if (allocation_count == 0) return 1;
#if defined(SLABJSON_WRAP_ALLOCATIONS)
    allocation_count = 0;
    tracking = true;
    void* (*volatile allocate)(std::size_t) = std::malloc;
    control = allocate(16);
    tracking = false;
    std::free(control);
    if (allocation_count == 0) return 1;
#endif
    allocation_count = 0;
    tracking = true;
    const bool correct = exercise_core();
    tracking = false;
    if (!correct || allocation_count != 0) {
        std::fprintf(stderr, "core success=%d, allocations=%zu\n", correct, allocation_count);
        return 1;
    }
    return 0;
}
