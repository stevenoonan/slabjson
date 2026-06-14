#include <slabjson/cjson_compat.hpp>

#include <limits>

#include <slabjson/serialize.hpp>

namespace slabjson {
namespace detail {

namespace {

[[nodiscard]] constexpr unsigned char fold_ascii(
    unsigned char character) noexcept
{
    return character >= 'A' && character <= 'Z'
        ? static_cast<unsigned char>(character + ('a' - 'A'))
        : character;
}

[[nodiscard]] bool ascii_case_equal(
    std::string_view left,
    std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (fold_ascii(static_cast<unsigned char>(left[index]))
            != fold_ascii(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

} // namespace

class CjsonAccess {
public:
    [[nodiscard]] static Result<Value> detach_from_array(
        Value array,
        std::size_t index) noexcept
    {
        Slab::Node* parent = node_for_parent(array, ValueType::Array);
        if (parent == nullptr) {
            return invalid_parent_error(array, ValueType::Array);
        }

        Slab::NodeId previous_id = Slab::kInvalidNodeId;
        Slab::NodeId child_id = parent->first_child;
        while (child_id != Slab::kInvalidNodeId && index > 0) {
            Slab::Node* child =
                array.slab_->node_for(child_id, array.generation_);
            if (child == nullptr) {
                return Error{ErrorCode::InternalError, 0};
            }
            previous_id = child_id;
            child_id = child->next_sibling;
            --index;
        }

        if (child_id == Slab::kInvalidNodeId) {
            return Error{ErrorCode::NotFound, 0};
        }
        return unlink_child(array, *parent, previous_id, child_id);
    }

    [[nodiscard]] static Result<Value> detach_from_object(
        Value object,
        std::string_view key,
        bool case_sensitive) noexcept
    {
        Slab::Node* parent = node_for_parent(object, ValueType::Object);
        if (parent == nullptr) {
            return invalid_parent_error(object, ValueType::Object);
        }

        Slab::NodeId previous_id = Slab::kInvalidNodeId;
        Slab::NodeId child_id = parent->first_child;
        while (child_id != Slab::kInvalidNodeId) {
            Slab::Node* child =
                object.slab_->node_for(child_id, object.generation_);
            if (child == nullptr) {
                return Error{ErrorCode::InternalError, 0};
            }

            const std::string_view child_key =
                object.slab_->view_string(child->key);
            const bool matches = case_sensitive
                ? child_key == key
                : ascii_case_equal(child_key, key);
            if (matches) {
                return unlink_child(
                    object,
                    *parent,
                    previous_id,
                    child_id);
            }

            previous_id = child_id;
            child_id = child->next_sibling;
        }
        return Error{ErrorCode::NotFound, 0};
    }

private:
    [[nodiscard]] static Slab::Node* node_for_parent(
        Value parent,
        ValueType expected_type) noexcept
    {
        if (parent.slab_ == nullptr) {
            return nullptr;
        }
        Slab::Node* node =
            parent.slab_->node_for(parent.id_, parent.generation_);
        return node != nullptr && node->type == expected_type
            ? node
            : nullptr;
    }

    [[nodiscard]] static Error invalid_parent_error(
        Value parent,
        ValueType expected_type) noexcept
    {
        if (!parent.valid()) {
            return Error{ErrorCode::InvalidHandle, 0};
        }
        return Error{
            parent.type() == expected_type
                ? ErrorCode::InternalError
                : ErrorCode::TypeMismatch,
            0,
        };
    }

    [[nodiscard]] static Result<Value> unlink_child(
        Value parent_value,
        Slab::Node& parent,
        Slab::NodeId previous_id,
        Slab::NodeId child_id) noexcept
    {
        Slab* slab = parent_value.slab_;
        Slab::Node* child =
            slab->node_for(child_id, parent_value.generation_);
        if (child == nullptr) {
            return Error{ErrorCode::InternalError, 0};
        }

        if (previous_id == Slab::kInvalidNodeId) {
            parent.first_child = child->next_sibling;
        } else {
            Slab::Node* previous =
                slab->node_for(previous_id, parent_value.generation_);
            if (previous == nullptr) {
                return Error{ErrorCode::InternalError, 0};
            }
            previous->next_sibling = child->next_sibling;
        }
        if (parent.last_child == child_id) {
            parent.last_child = previous_id;
        }

        child->parent = Slab::kInvalidNodeId;
        child->next_sibling = Slab::kInvalidNodeId;
        child->key = {};
        return slab->make_value(child_id);
    }
};

class CjsonCloner {
public:
    [[nodiscard]] static Result<Value> duplicate(
        Slab& destination,
        Value source,
        bool recurse) noexcept
    {
        if (source.slab_ == nullptr) {
            return Error{ErrorCode::InvalidHandle, 0};
        }
        if (!destination.valid()) {
            return Error{ErrorCode::InvalidArgument, 0};
        }

        const Slab* source_slab = source.slab_;
        const Slab::Node* source_root =
            source_slab->node_for(source.id_, source.generation_);
        if (source_root == nullptr
            || source_root->type == ValueType::Invalid) {
            return Error{ErrorCode::InvalidHandle, 0};
        }

        const Slab::Checkpoint checkpoint = destination.checkpoint();
        auto root_result =
            clone_node(destination, *source_slab, *source_root, false);
        if (!root_result) {
            destination.rollback(checkpoint);
            return root_result.error();
        }
        const Slab::NodeAllocation root_allocation = root_result.value();
        const Value root = destination.make_value(root_allocation.id);

        if (!recurse
            || (source_root->type != ValueType::Array
                && source_root->type != ValueType::Object)) {
            return root;
        }

        Slab::NodeId source_id = source.id_;
        const Slab::Node* source_node = source_root;
        Slab::NodeId destination_id = root_allocation.id;
        Slab::Node* destination_node = root_allocation.node;
        while (true) {
            if (source_node->first_child != Slab::kInvalidNodeId) {
                const Slab::NodeId child_source_id =
                    source_node->first_child;
                const Slab::Node* child_source =
                    source_slab->node_at_unchecked(child_source_id);
                auto child_result = clone_node(
                    destination,
                    *source_slab,
                    *child_source,
                    source_node->type == ValueType::Object);
                if (!child_result) {
                    destination.rollback(checkpoint);
                    return child_result.error();
                }
                const Slab::NodeAllocation child =
                    child_result.value();
                link_first_child(
                    *destination_node,
                    destination_id,
                    *child.node,
                    child.id);
                source_id = child_source_id;
                source_node = child_source;
                destination_id = child.id;
                destination_node = child.node;
                continue;
            }

            while (true) {
                if (source_id == source.id_) {
                    return root;
                }

                if (source_node->parent == Slab::kInvalidNodeId
                    || destination_node->parent
                        == Slab::kInvalidNodeId) {
                    destination.rollback(checkpoint);
                    return Error{ErrorCode::InternalError, 0};
                }

                if (source_node->next_sibling != Slab::kInvalidNodeId) {
                    const Slab::NodeId sibling_source_id =
                        source_node->next_sibling;
                    const Slab::Node* source_parent =
                        source_slab->node_at_unchecked(
                            source_node->parent);
                    const Slab::Node* sibling_source =
                        source_slab->node_at_unchecked(
                            sibling_source_id);
                    auto sibling_result = clone_node(
                        destination,
                        *source_slab,
                        *sibling_source,
                        source_parent->type == ValueType::Object);
                    if (!sibling_result) {
                        destination.rollback(checkpoint);
                        return sibling_result.error();
                    }
                    const Slab::NodeAllocation sibling =
                        sibling_result.value();
                    const Slab::NodeId destination_parent_id =
                        destination_node->parent;
                    Slab::Node* destination_parent =
                        destination.node_at_unchecked(
                            destination_parent_id);
                    link_next_sibling(
                        *destination_node,
                        *destination_parent,
                        destination_parent_id,
                        *sibling.node,
                        sibling.id);
                    source_id = sibling_source_id;
                    source_node = sibling_source;
                    destination_id = sibling.id;
                    destination_node = sibling.node;
                    break;
                }

                source_id = source_node->parent;
                source_node =
                    source_slab->node_at_unchecked(source_id);
                destination_id = destination_node->parent;
                destination_node =
                    destination.node_at_unchecked(destination_id);
            }
        }
    }

private:
    static void link_first_child(
        Slab::Node& parent,
        Slab::NodeId parent_id,
        Slab::Node& child,
        Slab::NodeId child_id) noexcept
    {
        parent.first_child = child_id;
        parent.last_child = child_id;
        child.parent = parent_id;
    }

    static void link_next_sibling(
        Slab::Node& previous,
        Slab::Node& parent,
        Slab::NodeId parent_id,
        Slab::Node& sibling,
        Slab::NodeId sibling_id) noexcept
    {
        previous.next_sibling = sibling_id;
        parent.last_child = sibling_id;
        sibling.parent = parent_id;
    }

    [[nodiscard]] static Result<Slab::NodeAllocation> clone_node(
        Slab& destination,
        const Slab& source_slab,
        const Slab::Node& source,
        bool copy_key) noexcept
    {
        if (source.type == ValueType::Invalid) {
            return Error{ErrorCode::InvalidHandle, 0};
        }

        auto allocation_result =
            destination.allocate_node_with_pointer(source.type);
        if (!allocation_result) {
            return allocation_result.error();
        }
        const Slab::NodeAllocation allocation =
            allocation_result.value();
        Slab::Node* node = allocation.node;

        node->number_kind = source.number_kind;
        node->payload = source.payload;

        if (source.type == ValueType::String) {
            auto string_result = destination.copy_string_trusted(
                source_slab.view_string(
                    source.payload.string_value));
            if (!string_result) {
                return string_result.error();
            }
            node->payload.string_value = string_result.value();
        }

        if (copy_key) {
            auto key_result = destination.copy_string_trusted(
                source_slab.view_string(source.key));
            if (!key_result) {
                return key_result.error();
            }
            node->key = key_result.value();
        }
        return allocation;
    }
};

} // namespace detail

namespace cjson {
namespace {

[[nodiscard]] Result<Item> container_value(
    Result<Object> result) noexcept
{
    if (!result) {
        return result.error();
    }
    return result.value().value();
}

[[nodiscard]] Result<Item> container_value(
    Result<Array> result) noexcept
{
    if (!result) {
        return result.error();
    }
    return result.value().value();
}

[[nodiscard]] Result<void> require_terminated_capacity(
    std::size_t json_size,
    std::span<char> output) noexcept
{
    if (json_size == std::numeric_limits<std::size_t>::max()
        || output.size() <= json_size) {
        return Error{ErrorCode::OutputCapacityExceeded, 0};
    }
    return {};
}

} // namespace

Result<Item> create_null(Slab& slab) noexcept
{
    return slab.make_null();
}

Result<Item> create_true(Slab& slab) noexcept
{
    return slab.make_bool(true);
}

Result<Item> create_false(Slab& slab) noexcept
{
    return slab.make_bool(false);
}

Result<Item> create_bool(Slab& slab, bool value) noexcept
{
    return slab.make_bool(value);
}

Result<Item> create_number(Slab& slab, std::int64_t value) noexcept
{
    return slab.make_number(value);
}

Result<Item> create_number(Slab& slab, std::uint64_t value) noexcept
{
    return slab.make_number(value);
}

Result<Item> create_number(Slab& slab, double value) noexcept
{
    return slab.make_number(value);
}

Result<Item> create_string(
    Slab& slab,
    std::string_view value) noexcept
{
    return slab.make_string(value);
}

Result<Item> create_array(Slab& slab) noexcept
{
    return container_value(slab.make_array());
}

Result<Item> create_object(Slab& slab) noexcept
{
    return container_value(slab.make_object());
}

Result<void> add_item_to_array(Item array, Item item) noexcept
{
    if (!array.valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    auto converted = array.as_array();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return converted->add(item);
}

Result<void> add_item_to_object(
    Item object,
    std::string_view key,
    Item item) noexcept
{
    if (!object.valid()) {
        return Error{ErrorCode::InvalidHandle, 0};
    }
    auto converted = object.as_object();
    if (!converted) {
        return Error{ErrorCode::TypeMismatch, 0};
    }
    return converted->add(key, item);
}

std::optional<Item> get_array_item(
    Item array,
    std::size_t index) noexcept
{
    auto converted = array.as_array();
    return converted ? converted->at(index) : std::nullopt;
}

std::size_t get_array_size(Item item) noexcept
{
    if (auto array = item.as_array()) {
        return array->size();
    }
    if (auto object = item.as_object()) {
        return object->size();
    }
    return 0;
}

std::optional<Item> get_object_item(
    Item object,
    std::string_view key) noexcept
{
    auto converted = object.as_object();
    if (!converted) {
        return std::nullopt;
    }
    for (ObjectMember member : *converted) {
        if (detail::ascii_case_equal(member.key, key)) {
            return member.value;
        }
    }
    return std::nullopt;
}

std::optional<Item> get_object_item_case_sensitive(
    Item object,
    std::string_view key) noexcept
{
    auto converted = object.as_object();
    return converted ? converted->find(key) : std::nullopt;
}

bool has_object_item(Item object, std::string_view key) noexcept
{
    return get_object_item(object, key).has_value();
}

bool is_invalid(Item item) noexcept
{
    return !item.valid();
}

bool is_false(Item item) noexcept
{
    auto value = item.as_bool();
    return value.has_value() && !*value;
}

bool is_true(Item item) noexcept
{
    return item.as_bool().value_or(false);
}

bool is_bool(Item item) noexcept
{
    return item.is_bool();
}

bool is_null(Item item) noexcept
{
    return item.is_null();
}

bool is_number(Item item) noexcept
{
    return item.is_number();
}

bool is_string(Item item) noexcept
{
    return item.is_string();
}

bool is_array(Item item) noexcept
{
    return item.is_array();
}

bool is_object(Item item) noexcept
{
    return item.is_object();
}

std::optional<std::string_view> get_string_value(Item item) noexcept
{
    return item.as_string();
}

std::optional<double> get_number_value(Item item) noexcept
{
    return item.as_number();
}

Result<Item> parse(
    Slab& slab,
    std::string_view input,
    ParseOptions options) noexcept
{
    return slabjson::parse(slab, input, options);
}

Result<std::size_t> print_unformatted(
    Item item,
    std::span<char> output) noexcept
{
    auto size_result = serialized_size(item);
    if (!size_result) {
        return size_result.error();
    }
    auto capacity_result =
        require_terminated_capacity(size_result.value(), output);
    if (!capacity_result) {
        return capacity_result.error();
    }

    auto result = serialize(item, output.first(size_result.value()));
    if (!result) {
        return result.error();
    }
    output[result.value()] = '\0';
    return result.value();
}

Result<std::size_t> print_pretty(
    Item item,
    std::span<char> output,
    std::uint8_t indent_spaces) noexcept
{
    auto size_result =
        serialized_size_pretty(item, indent_spaces);
    if (!size_result) {
        return size_result.error();
    }
    auto capacity_result =
        require_terminated_capacity(size_result.value(), output);
    if (!capacity_result) {
        return capacity_result.error();
    }

    auto result = serialize_pretty(
        item,
        output.first(size_result.value()),
        indent_spaces);
    if (!result) {
        return result.error();
    }
    output[result.value()] = '\0';
    return result.value();
}

Result<std::size_t> print_preallocated(
    Item item,
    std::span<char> output,
    bool formatted,
    std::uint8_t indent_spaces) noexcept
{
    return formatted
        ? print_pretty(item, output, indent_spaces)
        : print_unformatted(item, output);
}

Result<Item> duplicate(
    Slab& destination,
    Item item,
    bool recurse) noexcept
{
    return detail::CjsonCloner::duplicate(
        destination,
        item,
        recurse);
}

Result<Item> detach_item_from_array(
    Item array,
    std::size_t index) noexcept
{
    return detail::CjsonAccess::detach_from_array(array, index);
}

Result<void> delete_item_from_array(
    Item array,
    std::size_t index) noexcept
{
    auto result = detach_item_from_array(array, index);
    return result ? Result<void>{} : Result<void>{result.error()};
}

Result<Item> detach_item_from_object(
    Item object,
    std::string_view key) noexcept
{
    return detail::CjsonAccess::detach_from_object(
        object,
        key,
        false);
}

Result<Item> detach_item_from_object_case_sensitive(
    Item object,
    std::string_view key) noexcept
{
    return detail::CjsonAccess::detach_from_object(
        object,
        key,
        true);
}

Result<void> delete_item_from_object(
    Item object,
    std::string_view key) noexcept
{
    auto result = detach_item_from_object(object, key);
    return result ? Result<void>{} : Result<void>{result.error()};
}

void delete_all(Slab& slab) noexcept
{
    slab.reset();
}

} // namespace cjson
} // namespace slabjson
