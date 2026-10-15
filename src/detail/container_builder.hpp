#pragma once

#include <type_traits>
#include <utility>

#include <slabjson/slab.hpp>

namespace slabjson::detail {

class ContainerBuilder {
public:
    template <typename T, typename Container, typename Create, typename Attach>
    [[nodiscard]] static Result<T> add(
        Container& parent, Slab* slab, Create create, Attach attach) noexcept
    {
        auto current = parent.status();
        if (!current) {
            return slab == nullptr ? current.error() : slab->record_error(current.error());
        }

        const auto checkpoint = slab->checkpoint();
        auto child = create();
        if (!child) {
            return slab->record_error(child.error());
        }
        auto attached = attach(child.value());
        if (!attached) {
            slab->rollback(checkpoint);
            return attached.error();
        }
        if constexpr (std::is_void_v<T>) {
            return {};
        } else {
            return std::move(child).value();
        }
    }
};

} // namespace slabjson::detail
