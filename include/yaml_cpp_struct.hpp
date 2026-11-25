#ifndef YAML_CPP_STRUCT_HPP_
#define YAML_CPP_STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <forward_list>
#include <functional>
#include <istream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

#include <magic_enum.hpp>
#include <yaml-cpp/yaml.h>

#ifdef OPEN_YAML_TO_JSON

#include <nlohmann/json.hpp>

namespace yaml_cpp_struct {

inline nlohmann::json parse_scalar(const YAML::Node &node) {
    int i;
    long long ll;
    unsigned long long ull;
    double d;
    bool b;
    std::string s;

    if (YAML::convert<int>::decode(node, i))
        return i;
    if (YAML::convert<long long>::decode(node, ll))
        return ll;
    if (YAML::convert<unsigned long long>::decode(node, ull))
        return ull;
    if (YAML::convert<double>::decode(node, d))
        return d;
    if (YAML::convert<bool>::decode(node, b))
        return b;
    if (YAML::convert<std::string>::decode(node, s))
        return s;

    return nullptr;
}

inline nlohmann::json yaml2json(const YAML::Node &root) {
    switch (root.Type()) {
    case YAML::NodeType::Null:
        return nullptr;
    case YAML::NodeType::Scalar:
        return parse_scalar(root);
    case YAML::NodeType::Sequence: {
        nlohmann::json j = nlohmann::json::array();
        for (auto &&node : root)
            j.emplace_back(yaml2json(node));
        return j;
    }
    case YAML::NodeType::Map: {
        nlohmann::json j = nlohmann::json::object();
        for (auto &&it : root)
            j[it.first.as<std::string>()] = yaml2json(it.second);
        return j;
    }
    default:
        return nullptr;
    }
}

[[nodiscard]] inline nlohmann::json yaml_to_json(const std::string &str) {
    YAML::Node root = YAML::Load(str);
    return yaml2json(root);
}

} // namespace yaml_cpp_struct

#endif

namespace yaml_cpp_struct {

enum class ErrorCode {
    None,
    BadFile,
    ParseError,
    NotAMap,
    MissingField,
    InvalidField,
    BadEnum,
    OutOfRange,
    EmitError,
    EnvParseError,
    EnvDuplicate,
    EnvConflict,
};

struct Error {
    ErrorCode code{ErrorCode::None};
    std::string path;
    std::string message;
    int line{-1};
    int column{-1};
    std::string env_name;

    explicit operator bool() const noexcept { return code != ErrorCode::None; }
};

template <typename T> struct Result {
    std::optional<T> value;
    Error error;

    explicit operator bool() const noexcept { return value.has_value(); }

    T &operator*() { return *value; }

    const T &operator*() const { return *value; }

    T *operator->() { return &*value; }

    const T *operator->() const { return &*value; }
};

class Exception : public std::exception {
  public:
    explicit Exception(Error error) : m_error(std::move(error)) {}

    const Error &error() const noexcept { return m_error; }

    const char *what() const noexcept override { return m_error.message.c_str(); }

  private:
    Error m_error;
};

template <typename... Args> inline std::string string_format(const std::string &format, Args... args) {
    const int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if (size_s <= 0)
        throw std::runtime_error("Error during formatting.");
    const auto size = static_cast<std::size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[size]());
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

inline std::string format_error(const Error &error) {
    if (error.code == ErrorCode::None)
        return {};
    std::string message = error.message;
    if (!error.env_name.empty() && message.find(error.env_name) == std::string::npos)
        message += " [env " + error.env_name + "]";
    if (error.line >= 0 && error.column >= 0)
        message += string_format(" (line %d, column %d)", error.line + 1, error.column + 1);
    return message;
}

using EnvLookup = std::function<std::optional<std::string>(std::string_view)>;

inline EnvLookup process_env() {
    return [](std::string_view name) -> std::optional<std::string> {
        const std::string key(name);
#ifdef _WIN32
        char *buffer = nullptr;
        std::size_t size = 0;
        if (_dupenv_s(&buffer, &size, key.c_str()) == 0 && buffer != nullptr) {
            std::string value(buffer);
            std::free(buffer);
            return value;
        }
        return std::nullopt;
#else
        if (const char *value = std::getenv(key.c_str()))
            return std::string(value);
        return std::nullopt;
#endif
    };
}

struct EnvOptions {
    bool enabled{true};
    std::string prefix;
    std::string separator{"__"};
    bool uppercase{true};
    bool ignore_empty{true};
    bool parse_strings_as_yaml{false};
    std::optional<char> list_separator;
    EnvLookup lookup{process_env()};
    std::function<void(std::string_view, std::string_view)> on_applied;
};

// Set by YCS_ENV_PREFIX(T, "APP_").
template <typename T> inline constexpr std::string_view env_prefix = {};

namespace detail {

// YCS_DEFINE inside a struct injects this tag so the rest of the library can
// recognize the type without a namespace-scope YCS_ADD_STRUCT specialization.
template <typename T, typename = void> struct has_ycs_define : std::false_type {};

template <typename T> struct has_ycs_define<T, std::void_t<typename T::ycs_is_yaml_struct>> : std::true_type {};

template <typename T> struct is_std_vector : std::false_type {};

template <typename T, typename A> struct is_std_vector<std::vector<T, A>> : std::true_type {};

template <typename T> struct is_std_array : std::false_type {};

template <typename T, std::size_t N> struct is_std_array<std::array<T, N>> : std::true_type {};

template <typename T> struct is_std_list : std::false_type {};

template <typename T, typename A> struct is_std_list<std::list<T, A>> : std::true_type {};

template <typename T> struct is_std_map : std::false_type {};

template <typename K, typename V, typename C, typename A> struct is_std_map<std::map<K, V, C, A>> : std::true_type {};

template <typename T> struct is_std_unordered_map : std::false_type {};

template <typename K, typename V, typename H, typename P, typename A>
struct is_std_unordered_map<std::unordered_map<K, V, H, P, A>> : std::true_type {};

// Used in diagnostics.  YCS_ADD_STRUCT overrides it with the concrete type
// name; YCS_DEFINE cannot see its own name from inside the class.
template <typename T> struct struct_name {
    static constexpr const char *value = "struct";
};

} // namespace detail

// Set by YCS_ADD_STRUCT or detected from YCS_DEFINE.
template <typename T> inline constexpr bool is_yaml_struct = detail::has_ycs_define<T>::value;

namespace detail {

// True when decoding the type needs yaml_cpp_struct::node_as (because a YAML
// struct appears somewhere inside), instead of yaml-cpp's built-in convert<T>.
template <typename T> struct needs_struct_aware : std::bool_constant<is_yaml_struct<T>> {};

template <typename T> struct needs_struct_aware<std::optional<T>> : needs_struct_aware<T> {};

template <typename... T>
struct needs_struct_aware<std::tuple<T...>> : std::bool_constant<(needs_struct_aware<T>::value || ...)> {};

template <typename... T>
struct needs_struct_aware<std::variant<T...>> : std::bool_constant<(needs_struct_aware<T>::value || ...)> {};

template <typename T, typename A> struct needs_struct_aware<std::vector<T, A>> : needs_struct_aware<T> {};

template <typename T, typename A> struct needs_struct_aware<std::list<T, A>> : needs_struct_aware<T> {};

template <typename T, typename A> struct needs_struct_aware<std::deque<T, A>> : needs_struct_aware<T> {};

template <typename T, typename A> struct needs_struct_aware<std::forward_list<T, A>> : needs_struct_aware<T> {};

template <typename T, typename C, typename A> struct needs_struct_aware<std::set<T, C, A>> : needs_struct_aware<T> {};

template <typename T, typename H, typename P, typename A>
struct needs_struct_aware<std::unordered_set<T, H, P, A>> : needs_struct_aware<T> {};

template <typename T, std::size_t N> struct needs_struct_aware<std::array<T, N>> : needs_struct_aware<T> {};

template <typename K, typename V, typename C, typename A>
struct needs_struct_aware<std::map<K, V, C, A>>
    : std::bool_constant<needs_struct_aware<K>::value || needs_struct_aware<V>::value> {};

template <typename K, typename V, typename H, typename P, typename A>
struct needs_struct_aware<std::unordered_map<K, V, H, P, A>>
    : std::bool_constant<needs_struct_aware<K>::value || needs_struct_aware<V>::value> {};

} // namespace detail

template <typename T> inline EnvOptions env_options_for() {
    EnvOptions options;
    options.prefix = std::string(env_prefix<T>);
    options.enabled = !options.prefix.empty();
    return options;
}

template <typename T, typename Enable = void> struct is_optional : std::false_type {};

template <typename T> struct is_optional<std::optional<T>> : std::true_type {};

template <typename T> inline constexpr bool is_optional_v = is_optional<T>::value;

namespace detail {

inline YAML::Mark safe_mark(const YAML::Node &node) noexcept {
    try {
        return node.Mark();
    } catch (...) {
        return YAML::Mark::null_mark();
    }
}

// Views into a YAML scalar would dangle as soon as the document is destroyed,
// so they are rejected at compile time (including views nested in containers).
template <typename T> struct contains_string_view : std::false_type {};

template <> struct contains_string_view<std::string_view> : std::true_type {};

template <typename T, typename A> struct contains_string_view<std::vector<T, A>> : contains_string_view<T> {};

template <typename T, typename A> struct contains_string_view<std::list<T, A>> : contains_string_view<T> {};

template <typename T, typename A> struct contains_string_view<std::deque<T, A>> : contains_string_view<T> {};

template <typename T, typename A> struct contains_string_view<std::forward_list<T, A>> : contains_string_view<T> {};

template <typename T, std::size_t N> struct contains_string_view<std::array<T, N>> : contains_string_view<T> {};

template <typename T> struct contains_string_view<std::optional<T>> : contains_string_view<T> {};

template <typename... T>
struct contains_string_view<std::tuple<T...>> : std::bool_constant<(contains_string_view<T>::value || ...)> {};

template <typename... T>
struct contains_string_view<std::variant<T...>> : std::bool_constant<(contains_string_view<T>::value || ...)> {};

template <typename K, typename V>
struct contains_string_view<std::pair<K, V>>
    : std::bool_constant<contains_string_view<K>::value || contains_string_view<V>::value> {};

template <typename K, typename V, typename C, typename A>
struct contains_string_view<std::map<K, V, C, A>>
    : std::bool_constant<contains_string_view<K>::value || contains_string_view<V>::value> {};

template <typename K, typename V, typename H, typename P, typename A>
struct contains_string_view<std::unordered_map<K, V, H, P, A>>
    : std::bool_constant<contains_string_view<K>::value || contains_string_view<V>::value> {};

template <typename T, typename C, typename A>
struct contains_string_view<std::set<T, C, A>> : contains_string_view<T> {};

template <typename T, typename H, typename P, typename A>
struct contains_string_view<std::unordered_set<T, H, P, A>> : contains_string_view<T> {};

template <typename T> inline std::remove_cv_t<std::remove_reference_t<T>> node_as_value(const YAML::Node &node) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, uint8_t>) {
        const auto value = node.as<uint32_t>();
        if (value > 0xFFu) {
            const auto mark = safe_mark(node);
            throw Exception(Error{ErrorCode::OutOfRange,
                                  {},
                                  string_format("value %u out of range for uint8_t", static_cast<unsigned>(value)),
                                  mark.line,
                                  mark.column});
        }
        return static_cast<U>(static_cast<uint8_t>(value));
    } else if constexpr (std::is_same_v<U, int8_t>) {
        const auto value = node.as<int32_t>();
        if (value < -128 || value > 127) {
            const auto mark = safe_mark(node);
            throw Exception(Error{ErrorCode::OutOfRange,
                                  {},
                                  string_format("value %d out of range for int8_t", static_cast<int>(value)),
                                  mark.line,
                                  mark.column});
        }
        return static_cast<U>(static_cast<int8_t>(value));
    } else {
        return node.as<U>();
    }
}

template <typename T> inline void decode_struct(const YAML::Node &node, T &rhs, const char *struct_name);

template <typename T> inline YAML::Node encode_struct(const T &rhs);

template <typename T> inline T node_as_dispatch(const YAML::Node &node);

} // namespace detail

template <typename T> inline std::remove_cv_t<std::remove_reference_t<T>> node_as(const YAML::Node &node) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    static_assert(!detail::contains_string_view<U>::value,
                  "std::string_view fields are not supported because the view would dangle; use std::string");

    return detail::node_as_dispatch<U>(node);
}

namespace detail {

// Structs defined with YCS_DEFINE have no YAML::convert specialization, so
// route them (and containers that hold them) through the library decoders
// instead of yaml-cpp's as<T>().  Everything else keeps the old behaviour.
template <typename U> inline U node_as_dispatch(const YAML::Node &node) {
    if constexpr (is_yaml_struct<U>) {
        U value{};
        decode_struct(node, value, struct_name<U>::value);
        return value;
    } else if constexpr (is_std_vector<U>::value && needs_struct_aware<U>::value) {
        if (!node.IsSequence())
            throw YAML::TypedBadConversion<U>(safe_mark(node));
        U result;
        result.reserve(node.size());
        for (const auto &element : node)
            result.push_back(yaml_cpp_struct::node_as<typename U::value_type>(element));
        return result;
    } else if constexpr (is_std_list<U>::value && needs_struct_aware<U>::value) {
        if (!node.IsSequence())
            throw YAML::TypedBadConversion<U>(safe_mark(node));
        U result;
        for (const auto &element : node)
            result.push_back(yaml_cpp_struct::node_as<typename U::value_type>(element));
        return result;
    } else if constexpr (is_std_array<U>::value && needs_struct_aware<U>::value) {
        constexpr std::size_t size = std::tuple_size<U>::value;
        if (!node.IsSequence() || node.size() != size)
            throw YAML::TypedBadConversion<U>(safe_mark(node));
        U result{};
        for (std::size_t i = 0; i < size; ++i)
            result[i] = yaml_cpp_struct::node_as<typename U::value_type>(node[i]);
        return result;
    } else if constexpr ((is_std_map<U>::value || is_std_unordered_map<U>::value) && needs_struct_aware<U>::value) {
        if (!node.IsMap())
            throw YAML::TypedBadConversion<U>(safe_mark(node));
        U result;
        for (const auto &entry : node)
            result.emplace(yaml_cpp_struct::node_as<typename U::key_type>(entry.first),
                           yaml_cpp_struct::node_as<typename U::mapped_type>(entry.second));
        return result;
    } else {
        // An empty document produces a null node without an underlying node,
        // and yaml-cpp's as<T>() rejects it before reaching convert<T>::decode().
        // Materialize the null node so convert<T> can handle it.
        if (node.IsDefined() && node.Type() == YAML::NodeType::Null)
            return node_as_value<U>(YAML::Node(YAML::NodeType::Null));
        return node_as_value<U>(node);
    }
}

// yaml-cpp encodes (signed/unsigned) char by streaming the raw character,
// which is not what users expect for 8-bit integer fields.  Encode them as
// numbers instead.
template <typename T> inline YAML::Node to_node_impl(const T &v) {
    using U = std::remove_cv_t<std::remove_reference_t<T>>;
    if constexpr (std::is_same_v<U, uint8_t>) {
        return YAML::Node(static_cast<uint32_t>(v));
    } else if constexpr (std::is_same_v<U, int8_t>) {
        return YAML::Node(static_cast<int32_t>(v));
    } else if constexpr (is_yaml_struct<U>) {
        return encode_struct<U>(v);
    } else if constexpr (is_std_vector<U>::value && needs_struct_aware<U>::value) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto &element : v)
            node.push_back(to_node_impl(element));
        return node;
    } else if constexpr (is_std_list<U>::value && needs_struct_aware<U>::value) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto &element : v)
            node.push_back(to_node_impl(element));
        return node;
    } else if constexpr (is_std_array<U>::value && needs_struct_aware<U>::value) {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto &element : v)
            node.push_back(to_node_impl(element));
        return node;
    } else if constexpr ((is_std_map<U>::value || is_std_unordered_map<U>::value) && needs_struct_aware<U>::value) {
        YAML::Node node(YAML::NodeType::Map);
        for (const auto &[key, value] : v)
            node.force_insert(to_node_impl(key), to_node_impl(value));
        return node;
    } else {
        return YAML::Node(v);
    }
}

} // namespace detail

template <typename... T, std::size_t... I>
inline std::tuple<T...> yaml_node_to_tuple(const YAML::Node &node, std::index_sequence<I...>) {
    return std::make_tuple(yaml_cpp_struct::node_as<T>(node[I])...);
}

template <typename... T, std::size_t... I>
inline YAML::Node tuple_to_yaml_node(const std::tuple<T...> &tup, std::index_sequence<I...>) {
    YAML::Node node(YAML::NodeType::Sequence);
    (node.push_back(yaml_cpp_struct::detail::to_node_impl(std::get<I>(tup))), ...);
    return node;
}

namespace detail {

// Primary template; YCS_ADD_STRUCT specializes it, while YCS_DEFINE provides
// the same information as members of the struct itself.
template <typename T> struct field_policies {
    static bool apply_default(const char *name, T &obj) { return obj.ycs_apply_default(name); }

    static const char *env_name(const char *member_name) { return T::ycs_env_name(member_name); }
};

inline YAML::Node struct_node_as_map(const YAML::Node &node, const char *struct_name) {
    if (!node || node.IsNull())
        return YAML::Node(YAML::NodeType::Map);
    if (!node.IsMap()) {
        const auto mark = safe_mark(node);
        throw Exception(Error{ErrorCode::NotAMap,
                              {},
                              string_format("yaml node for %s must be a map", struct_name),
                              mark.line,
                              mark.column});
    }
    return node;
}

inline Exception missing_field_error(const char *key, const char *struct_name, const YAML::Node &node) {
    const auto mark = safe_mark(node);
    return Exception(Error{ErrorCode::MissingField, key, string_format("missing field `%s` in %s", key, struct_name),
                           mark.line, mark.column});
}

// YCS_ADD_STRUCT specializes this with a generated field visitor; YCS_DEFINE
// structs expose the same visitor as a member instead.
template <typename T> struct field_access {
    template <typename F> static void for_each(T &obj, F &&f) { obj.ycs_visit(f); }

    template <typename F> static void for_each(const T &obj, F &&f) { obj.ycs_visit(f); }
};

template <typename Field>
inline void decode_field(const YAML::Node &parent, const char *key, const char *struct_name, Field &value) {
    using ToType = std::decay_t<Field>;
    const YAML::Node field = parent[key];

    try {
        value = yaml_cpp_struct::node_as<ToType>(field);
    } catch (const Exception &e) {
        Error error = e.error();
        error.path = error.path.empty() ? std::string(key) : std::string(key) + "." + error.path;
        error.message = string_format("invalid field `%s` in %s: %s", key, struct_name, error.message.c_str());
        throw Exception(std::move(error));
    } catch (const YAML::Exception &e) {
        throw Exception(Error{ErrorCode::InvalidField, key,
                              string_format("invalid field `%s` in %s: %s", key, struct_name, e.msg.c_str()),
                              e.mark.line, e.mark.column});
    } catch (const std::exception &e) {
        const auto mark = safe_mark(field);
        throw Exception(Error{ErrorCode::InvalidField, key,
                              string_format("invalid field `%s` in %s: %s", key, struct_name, e.what()), mark.line,
                              mark.column});
    }
}

template <typename T> inline void decode_struct(const YAML::Node &node, T &rhs, const char *struct_name) {
    const YAML::Node map = struct_node_as_map(node, struct_name);

    field_access<T>::for_each(rhs, [&](const char *name, auto &value) {
        using FieldType = std::decay_t<decltype(value)>;

        const YAML::Node field = map[name];
        if (!field) {
            if (!field_policies<T>::apply_default(name, rhs))
                throw missing_field_error(name, struct_name, node);
        } else if (field.IsNull() && !is_optional_v<FieldType>) {
            const auto mark = safe_mark(field);
            throw Exception(Error{ErrorCode::InvalidField, name,
                                  string_format("field `%s` in %s is null", name, struct_name), mark.line,
                                  mark.column});
        } else {
            decode_field(map, name, struct_name, value);
        }
    });
}

template <typename T> inline YAML::Node encode_struct(const T &rhs) {
    YAML::Node node(YAML::NodeType::Map);
    field_access<T>::for_each(
        rhs, [&](const char *name, auto &value) { node[name] = yaml_cpp_struct::detail::to_node_impl(value); });
    return node;
}

template <typename T> struct unwrap_optional {
    using type = T;
};

template <typename T> struct unwrap_optional<std::optional<T>> {
    using type = T;
};

template <typename T> using unwrap_optional_t = typename unwrap_optional<std::decay_t<T>>::type;

template <typename T> struct is_string_like : std::false_type {};

template <> struct is_string_like<std::string> : std::true_type {};

template <> struct is_string_like<std::filesystem::path> : std::true_type {};

template <typename T> struct is_sequence_container : std::false_type {};

template <typename T, typename A> struct is_sequence_container<std::vector<T, A>> : std::true_type {};

template <typename T, typename A> struct is_sequence_container<std::list<T, A>> : std::true_type {};

template <typename T, typename A> struct is_sequence_container<std::deque<T, A>> : std::true_type {};

template <typename T, typename A> struct is_sequence_container<std::forward_list<T, A>> : std::true_type {};

template <typename T, typename C, typename A> struct is_sequence_container<std::set<T, C, A>> : std::true_type {};

template <typename T, typename H, typename P, typename A>
struct is_sequence_container<std::unordered_set<T, H, P, A>> : std::true_type {};

template <typename T, std::size_t N> struct is_sequence_container<std::array<T, N>> : std::true_type {};

template <typename T> struct is_sequence_container<std::valarray<T>> : std::true_type {};

template <typename T> struct is_optional_yaml_struct : std::false_type {};

template <typename T> struct is_optional_yaml_struct<std::optional<T>> : std::bool_constant<is_yaml_struct<T>> {};

inline std::string normalize_env_segment(std::string_view segment, const EnvOptions &options) {
    std::string result(segment);
    if (options.uppercase) {
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    }
    return result;
}

inline std::string env_path_segment(const std::string &parent, std::string_view name, const EnvOptions &options) {
    const std::string segment = normalize_env_segment(name, options);
    if (parent.empty())
        return segment;
    return parent + options.separator + segment;
}

inline Exception env_error(ErrorCode code, std::string env_name, std::string path, std::string message) {
    Error error;
    error.code = code;
    error.env_name = std::move(env_name);
    error.path = std::move(path);
    error.message = std::move(message);
    return Exception(std::move(error));
}

template <typename Field>
inline void validate_env_node(const YAML::Node &node, const std::string &name, const std::string &path) {
    try {
        (void)yaml_cpp_struct::node_as<Field>(node);
    } catch (const std::exception &e) {
        throw env_error(
            ErrorCode::EnvParseError, name, path,
            string_format("environment variable %s for field %s is invalid: %s", name.c_str(), path.c_str(), e.what()));
    }
}

template <typename Field>
inline YAML::Node parse_env_value(std::string_view raw, const EnvOptions &options, std::string_view env_name,
                                  std::string_view field_path) {
    using U = unwrap_optional_t<Field>;
    const std::string name(env_name);
    const std::string path(field_path);

    if constexpr (std::is_same_v<U, char>) {
        if (raw.size() != 1) {
            throw env_error(ErrorCode::EnvParseError, name, path,
                            string_format("environment variable %s for field %s must contain exactly one character",
                                          name.c_str(), path.c_str()));
        }
        YAML::Node node{std::string(raw)};
        validate_env_node<Field>(node, name, path);
        return node;
    } else {
        if constexpr (is_string_like<U>::value) {
            if (!options.parse_strings_as_yaml) {
                YAML::Node node{std::string(raw)};
                validate_env_node<Field>(node, name, path);
                return node;
            }
        }

        YAML::Node node;
        try {
            node = YAML::Load(std::string(raw));
        } catch (const YAML::Exception &e) {
            throw env_error(ErrorCode::EnvParseError, name, path,
                            string_format("environment variable %s for field %s cannot be parsed as YAML: %s",
                                          name.c_str(), path.c_str(), e.msg.c_str()));
        }

        if constexpr (is_sequence_container<U>::value) {
            if (options.list_separator && !node.IsSequence() && node.IsScalar()) {
                YAML::Node sequence(YAML::NodeType::Sequence);
                const std::string text = node.Scalar();
                std::size_t start = 0;
                while (true) {
                    const std::size_t pos = text.find(*options.list_separator, start);
                    if (pos == std::string::npos) {
                        sequence.push_back(text.substr(start));
                        break;
                    }
                    sequence.push_back(text.substr(start, pos - start));
                    start = pos + 1;
                }
                node = sequence;
            }
        }

        validate_env_node<Field>(node, name, path);
        return node;
    }
}

struct EnvBinding {
    std::string env_name;
    std::string field_path;
    std::vector<std::string> keys;
    YAML::Node value;
};

inline void record_env_binding(std::vector<EnvBinding> &bindings, std::unordered_map<std::string, std::string> &owners,
                               EnvBinding binding) {
    const auto existing = owners.find(binding.env_name);
    if (existing != owners.end()) {
        throw env_error(ErrorCode::EnvDuplicate, binding.env_name, binding.field_path,
                        string_format("environment variable %s is bound to both %s and %s", binding.env_name.c_str(),
                                      existing->second.c_str(), binding.field_path.c_str()));
    }
    owners.emplace(binding.env_name, binding.field_path);
    bindings.push_back(std::move(binding));
}

template <typename T>
inline void collect_env_bindings(const EnvOptions &options, const EnvLookup &lookup, const std::string &naming_path,
                                 const std::string &field_path, bool explicit_base, std::vector<std::string> &keys,
                                 std::vector<EnvBinding> &bindings,
                                 std::unordered_map<std::string, std::string> &owners) {
    T probe{};
    field_access<T>::for_each(probe, [&](const char *name, auto &value) {
        using Field = std::decay_t<decltype(value)>;

        const std::string child_naming = env_path_segment(naming_path, name, options);
        const std::string child_field = field_path.empty() ? std::string(name) : field_path + "." + name;
        const char *explicit_name = field_policies<T>::env_name(name);
        const std::string env_name =
            explicit_name ? std::string(explicit_name) : (explicit_base ? child_naming : options.prefix + child_naming);

        if (auto raw = lookup(env_name)) {
            if (!(options.ignore_empty && raw->empty())) {
                keys.push_back(name);
                EnvBinding binding;
                binding.env_name = env_name;
                binding.field_path = child_field;
                binding.keys = keys;
                binding.value = parse_env_value<Field>(*raw, options, env_name, child_field);
                keys.pop_back();
                record_env_binding(bindings, owners, std::move(binding));
                return;
            }
        }

        const std::string nested_naming = explicit_name ? std::string(explicit_name) : child_naming;
        const bool nested_explicit_base = explicit_base || (explicit_name != nullptr);

        if constexpr (is_yaml_struct<Field>) {
            keys.push_back(name);
            collect_env_bindings<Field>(options, lookup, nested_naming, child_field, nested_explicit_base, keys,
                                        bindings, owners);
            keys.pop_back();
        } else if constexpr (is_optional_yaml_struct<Field>::value) {
            keys.push_back(name);
            collect_env_bindings<typename Field::value_type>(options, lookup, nested_naming, child_field,
                                                             nested_explicit_base, keys, bindings, owners);
            keys.pop_back();
        }
    });
}

inline void apply_env_bindings(YAML::Node root, const std::vector<EnvBinding> &bindings, const EnvOptions &options) {
    for (const auto &binding : bindings) {
        YAML::Node node = root;

        for (std::size_t i = 0; i + 1 < binding.keys.size(); ++i) {
            const std::string &key = binding.keys[i];
            YAML::Node child = node[key];
            if (!child) {
                node[key] = YAML::Node(YAML::NodeType::Map);
                child = node[key];
            } else if (!child.IsMap()) {
                throw env_error(ErrorCode::EnvConflict, binding.env_name, binding.field_path,
                                string_format("environment variable %s requires `%s` to be a map",
                                              binding.env_name.c_str(), binding.field_path.c_str()));
            }
            node.reset(child);
        }

        node[binding.keys.back()] = binding.value;
        if (options.on_applied)
            options.on_applied(binding.env_name, binding.field_path);
    }
}

template <typename T> inline void apply_env_options(YAML::Node &node, const EnvOptions &options) {
    if (!options.enabled)
        return;

    if constexpr (is_yaml_struct<T>) {
        if (!node || node.IsNull()) {
            node.reset(YAML::Node(YAML::NodeType::Map));
        } else if (!node.IsMap()) {
            // Let the decoder report NotAMap; env cannot be applied to a scalar root.
            return;
        }

        std::vector<EnvBinding> bindings;
        std::unordered_map<std::string, std::string> owners;
        std::vector<std::string> keys;
        collect_env_bindings<T>(options, options.lookup, "", "", false, keys, bindings, owners);
        apply_env_bindings(node, bindings, options);
    }
}

} // namespace detail

template <typename T> inline Result<T> from_node(const YAML::Node &node) {
    try {
        return Result<T>{yaml_cpp_struct::node_as<T>(node), Error{}};
    } catch (const Exception &e) {
        return Result<T>{std::nullopt, e.error()};
    } catch (const YAML::Exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::InvalidField, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::InvalidField, {}, e.what(), -1, -1}};
    }
}

namespace detail {

template <typename T> inline Result<T> decode_with_env(YAML::Node &node, const EnvOptions &options) {
    try {
        apply_env_options<T>(node, options);
    } catch (const Exception &e) {
        return Result<T>{std::nullopt, e.error()};
    } catch (const YAML::Exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::EnvParseError, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::EnvParseError, {}, e.what(), -1, -1}};
    }
    return from_node<T>(node);
}

template <typename T>
inline Result<std::vector<T>> decode_all_with_env(std::vector<YAML::Node> &nodes, const EnvOptions &options) {
    try {
        std::vector<T> values;
        values.reserve(nodes.size());
        for (auto &node : nodes) {
            apply_env_options<T>(node, options);
            values.push_back(yaml_cpp_struct::node_as<T>(node));
        }
        return Result<std::vector<T>>{std::move(values), Error{}};
    } catch (const Exception &e) {
        return Result<std::vector<T>>{std::nullopt, e.error()};
    } catch (const YAML::Exception &e) {
        return Result<std::vector<T>>{std::nullopt,
                                      Error{ErrorCode::InvalidField, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<std::vector<T>>{std::nullopt, Error{ErrorCode::InvalidField, {}, e.what(), -1, -1}};
    }
}

} // namespace detail

template <typename T>
inline Result<T> from_yaml(const std::string &file, const EnvOptions &options = env_options_for<T>()) {
    YAML::Node node;
    try {
        node = YAML::LoadFile(file);
    } catch (const YAML::BadFile &) {
        return Result<T>{std::nullopt,
                         Error{ErrorCode::BadFile, {}, string_format("%s not found or broken", file.c_str()), -1, -1}};
    } catch (const YAML::Exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::ParseError, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::ParseError, {}, e.what(), -1, -1}};
    }
    return detail::decode_with_env<T>(node, options);
}

template <typename T>
inline Result<T> from_yaml_str(const std::string &yaml, const EnvOptions &options = env_options_for<T>()) {
    YAML::Node node;
    try {
        node = YAML::Load(yaml);
    } catch (const YAML::Exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::ParseError, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::ParseError, {}, e.what(), -1, -1}};
    }
    return detail::decode_with_env<T>(node, options);
}

template <typename T>
inline Result<T> from_yaml_stream(std::istream &in, const EnvOptions &options = env_options_for<T>()) {
    YAML::Node node;
    try {
        node = YAML::Load(in);
    } catch (const YAML::Exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::ParseError, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<T>{std::nullopt, Error{ErrorCode::ParseError, {}, e.what(), -1, -1}};
    }
    return detail::decode_with_env<T>(node, options);
}

template <typename T>
inline Result<std::vector<T>> from_yaml_all(const std::string &file, const EnvOptions &options = env_options_for<T>()) {
    std::vector<YAML::Node> nodes;
    try {
        nodes = YAML::LoadAllFromFile(file);
    } catch (const YAML::BadFile &) {
        return Result<std::vector<T>>{
            std::nullopt, Error{ErrorCode::BadFile, {}, string_format("%s not found or broken", file.c_str()), -1, -1}};
    } catch (const YAML::Exception &e) {
        return Result<std::vector<T>>{std::nullopt,
                                      Error{ErrorCode::ParseError, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<std::vector<T>>{std::nullopt, Error{ErrorCode::ParseError, {}, e.what(), -1, -1}};
    }
    return detail::decode_all_with_env<T>(nodes, options);
}

template <typename T>
inline Result<std::vector<T>> from_yaml_all_str(const std::string &yaml,
                                                const EnvOptions &options = env_options_for<T>()) {
    std::vector<YAML::Node> nodes;
    try {
        nodes = YAML::LoadAll(yaml);
    } catch (const YAML::Exception &e) {
        return Result<std::vector<T>>{std::nullopt,
                                      Error{ErrorCode::ParseError, {}, e.msg, e.mark.line, e.mark.column}};
    } catch (const std::exception &e) {
        return Result<std::vector<T>>{std::nullopt, Error{ErrorCode::ParseError, {}, e.what(), -1, -1}};
    }
    return detail::decode_all_with_env<T>(nodes, options);
}

template <typename T> inline Result<T> from_env(const EnvOptions &options = env_options_for<T>()) {
    YAML::Node node(YAML::NodeType::Map);
    return detail::decode_with_env<T>(node, options);
}

template <typename T> inline Result<YAML::Node> to_node(const T &obj) {
    try {
        return Result<YAML::Node>{yaml_cpp_struct::detail::to_node_impl(obj), Error{}};
    } catch (const Exception &e) {
        return Result<YAML::Node>{std::nullopt, e.error()};
    } catch (const std::exception &e) {
        return Result<YAML::Node>{std::nullopt, Error{ErrorCode::EmitError, {}, e.what(), -1, -1}};
    }
}

template <typename T> inline Result<std::string> to_yaml(const T &obj) {
    try {
        YAML::Emitter emitter;
        emitter << yaml_cpp_struct::detail::to_node_impl(obj);
        if (!emitter.good())
            return Result<std::string>{std::nullopt, Error{ErrorCode::EmitError, {}, emitter.GetLastError(), -1, -1}};
        return Result<std::string>{std::string(emitter.c_str()), Error{}};
    } catch (const Exception &e) {
        return Result<std::string>{std::nullopt, e.error()};
    } catch (const std::exception &e) {
        return Result<std::string>{std::nullopt, Error{ErrorCode::EmitError, {}, e.what(), -1, -1}};
    }
}

} // namespace yaml_cpp_struct

namespace YAML {

template <typename K, typename V> struct convert<std::unordered_map<K, V>> {
    static bool decode(const Node &node, std::unordered_map<K, V> &rhs) {
        if (!node.IsMap())
            return false;
        rhs.clear();
        for (auto &it : node)
            rhs.emplace(it.first.template as<K>(), it.second.template as<V>());
        return true;
    }

    static Node encode(const std::unordered_map<K, V> &rhs) {
        Node node(NodeType::Map);
        for (auto &[k, v] : rhs)
            node.force_insert(yaml_cpp_struct::detail::to_node_impl(k), yaml_cpp_struct::detail::to_node_impl(v));
        return node;
    }
};

template <typename R, typename P> struct convert<std::chrono::duration<R, P>> {
    static bool decode(const Node &node, std::chrono::duration<R, P> &rhs) {
        if (!node.IsScalar())
            return false;
        rhs = std::chrono::duration<R, P>(node.template as<R>());
        return true;
    }

    static Node encode(const std::chrono::duration<R, P> &rhs) {
        return yaml_cpp_struct::detail::to_node_impl(rhs.count());
    }
};

template <typename V, typename H, typename P, typename A> struct convert<std::unordered_set<V, H, P, A>> {
    static bool decode(const Node &node, std::unordered_set<V, H, P, A> &rhs) {
        if (!node.IsSequence())
            return false;
        rhs.clear();
        for (auto &it : node)
            rhs.emplace(yaml_cpp_struct::node_as<V>(it));
        return true;
    }

    static Node encode(const std::unordered_set<V, H, P, A> &rhs) {
        Node node(NodeType::Sequence);
        for (auto &it : rhs)
            node.push_back(yaml_cpp_struct::detail::to_node_impl(it));
        return node;
    }
};

template <typename V, typename C, typename A> struct convert<std::set<V, C, A>> {
    static bool decode(const Node &node, std::set<V, C, A> &rhs) {
        if (!node.IsSequence())
            return false;
        rhs.clear();
        for (auto &it : node)
            rhs.emplace(yaml_cpp_struct::node_as<V>(it));
        return true;
    }

    static Node encode(const std::set<V, C, A> &rhs) {
        Node node(NodeType::Sequence);
        for (auto &it : rhs)
            node.push_back(yaml_cpp_struct::detail::to_node_impl(it));
        return node;
    }
};

template <typename T, typename A> struct convert<std::deque<T, A>> {
    static bool decode(const Node &node, std::deque<T, A> &rhs) {
        if (!node.IsSequence())
            return false;
        rhs.clear();
        for (auto &it : node)
            rhs.push_back(yaml_cpp_struct::node_as<T>(it));
        return true;
    }

    static Node encode(const std::deque<T, A> &rhs) {
        Node node(NodeType::Sequence);
        for (auto &it : rhs)
            node.push_back(yaml_cpp_struct::detail::to_node_impl(it));
        return node;
    }
};

template <typename T, typename A> struct convert<std::forward_list<T, A>> {
    static bool decode(const Node &node, std::forward_list<T, A> &rhs) {
        if (!node.IsSequence())
            return false;
        rhs.clear();
        auto tail = rhs.before_begin();
        for (auto &it : node)
            tail = rhs.insert_after(tail, yaml_cpp_struct::node_as<T>(it));
        return true;
    }

    static Node encode(const std::forward_list<T, A> &rhs) {
        Node node(NodeType::Sequence);
        for (auto &it : rhs)
            node.push_back(yaml_cpp_struct::detail::to_node_impl(it));
        return node;
    }
};

namespace ycs_detail {

template <typename Container> inline bool decode_numeric_sequence(const Node &node, Container &rhs) {
    if (!node.IsSequence())
        return false;
    rhs.clear();
    for (auto &it : node)
        rhs.push_back(yaml_cpp_struct::node_as<typename Container::value_type>(it));
    return true;
}

template <typename Container> inline Node encode_numeric_sequence(const Container &rhs) {
    Node node(NodeType::Sequence);
    for (auto &it : rhs)
        node.push_back(yaml_cpp_struct::detail::to_node_impl(it));
    return node;
}

} // namespace ycs_detail

// yaml-cpp's native container conversions encode (signed/unsigned) char as a
// character.  Override the 8-bit cases so they stay numeric.
template <typename A> struct convert<std::vector<uint8_t, A>> {
    static bool decode(const Node &node, std::vector<uint8_t, A> &rhs) {
        return ycs_detail::decode_numeric_sequence(node, rhs);
    }
    static Node encode(const std::vector<uint8_t, A> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <typename A> struct convert<std::vector<int8_t, A>> {
    static bool decode(const Node &node, std::vector<int8_t, A> &rhs) {
        return ycs_detail::decode_numeric_sequence(node, rhs);
    }
    static Node encode(const std::vector<int8_t, A> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <typename A> struct convert<std::list<uint8_t, A>> {
    static bool decode(const Node &node, std::list<uint8_t, A> &rhs) {
        return ycs_detail::decode_numeric_sequence(node, rhs);
    }
    static Node encode(const std::list<uint8_t, A> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <typename A> struct convert<std::list<int8_t, A>> {
    static bool decode(const Node &node, std::list<int8_t, A> &rhs) {
        return ycs_detail::decode_numeric_sequence(node, rhs);
    }
    static Node encode(const std::list<int8_t, A> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <std::size_t N> struct convert<std::array<uint8_t, N>> {
    static bool decode(const Node &node, std::array<uint8_t, N> &rhs) {
        if (!node.IsSequence() || node.size() != N)
            return false;
        for (std::size_t i = 0; i < N; ++i)
            rhs[i] = yaml_cpp_struct::node_as<uint8_t>(node[i]);
        return true;
    }
    static Node encode(const std::array<uint8_t, N> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <std::size_t N> struct convert<std::array<int8_t, N>> {
    static bool decode(const Node &node, std::array<int8_t, N> &rhs) {
        if (!node.IsSequence() || node.size() != N)
            return false;
        for (std::size_t i = 0; i < N; ++i)
            rhs[i] = yaml_cpp_struct::node_as<int8_t>(node[i]);
        return true;
    }
    static Node encode(const std::array<int8_t, N> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <> struct convert<std::valarray<uint8_t>> {
    static bool decode(const Node &node, std::valarray<uint8_t> &rhs) {
        if (!node.IsSequence())
            return false;
        rhs.resize(node.size());
        for (std::size_t i = 0; i < node.size(); ++i)
            rhs[i] = yaml_cpp_struct::node_as<uint8_t>(node[i]);
        return true;
    }
    static Node encode(const std::valarray<uint8_t> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <> struct convert<std::valarray<int8_t>> {
    static bool decode(const Node &node, std::valarray<int8_t> &rhs) {
        if (!node.IsSequence())
            return false;
        rhs.resize(node.size());
        for (std::size_t i = 0; i < node.size(); ++i)
            rhs[i] = yaml_cpp_struct::node_as<int8_t>(node[i]);
        return true;
    }
    static Node encode(const std::valarray<int8_t> &rhs) { return ycs_detail::encode_numeric_sequence(rhs); }
};

template <typename K, typename C, typename A> struct convert<std::map<K, uint8_t, C, A>> {
    static bool decode(const Node &node, std::map<K, uint8_t, C, A> &rhs) {
        if (!node.IsMap())
            return false;
        rhs.clear();
        for (auto &it : node)
            rhs.emplace(it.first.template as<K>(), yaml_cpp_struct::node_as<uint8_t>(it.second));
        return true;
    }
    static Node encode(const std::map<K, uint8_t, C, A> &rhs) {
        Node node(NodeType::Map);
        for (auto &[k, v] : rhs)
            node.force_insert(yaml_cpp_struct::detail::to_node_impl(k), yaml_cpp_struct::detail::to_node_impl(v));
        return node;
    }
};

template <typename K, typename C, typename A> struct convert<std::map<K, int8_t, C, A>> {
    static bool decode(const Node &node, std::map<K, int8_t, C, A> &rhs) {
        if (!node.IsMap())
            return false;
        rhs.clear();
        for (auto &it : node)
            rhs.emplace(it.first.template as<K>(), yaml_cpp_struct::node_as<int8_t>(it.second));
        return true;
    }
    static Node encode(const std::map<K, int8_t, C, A> &rhs) {
        Node node(NodeType::Map);
        for (auto &[k, v] : rhs)
            node.force_insert(yaml_cpp_struct::detail::to_node_impl(k), yaml_cpp_struct::detail::to_node_impl(v));
        return node;
    }
};

template <> struct convert<std::filesystem::path> {
    static bool decode(const Node &node, std::filesystem::path &rhs) {
        if (!node.IsScalar())
            return false;
        rhs = node.Scalar();
        return true;
    }

    static Node encode(const std::filesystem::path &rhs) { return Node(rhs.string()); }
};

template <> struct convert<std::monostate> {
    static bool decode(const Node &node, std::monostate &) { return node.IsNull(); }

    static Node encode(const std::monostate &) { return Node(NodeType::Null); }
};

template <typename T> struct convert<std::optional<T>> {
    static bool decode(const Node &node, std::optional<T> &rhs) {
        if (node.IsNull())
            rhs = std::nullopt;
        else
            rhs = yaml_cpp_struct::node_as<T>(node);
        return true;
    }

    static Node encode(const std::optional<T> &rhs) {
        if (rhs.has_value())
            return yaml_cpp_struct::detail::to_node_impl(rhs.value());
        else
            return Node{};
    }
};

template <typename... T> struct convert<std::tuple<T...>> {
    static bool decode(const Node &node, std::tuple<T...> &rhs) {
        if (!node.IsSequence() || (node.size() != sizeof...(T)))
            return false;
        rhs = yaml_cpp_struct::yaml_node_to_tuple<T...>(node, std::index_sequence_for<T...>{});
        return true;
    }

    static Node encode(const std::tuple<T...> &rhs) {
        return yaml_cpp_struct::tuple_to_yaml_node<T...>(rhs, std::index_sequence_for<T...>{});
    }
};

template <typename... T> struct convert<std::variant<T...>> {
    static bool decode(const Node &node, std::variant<T...> &rhs) {
        bool flag = false;
        std::string last_error;
        (
            [&]() {
                if (flag)
                    return;
                try {
                    rhs = yaml_cpp_struct::node_as<T>(node);
                    flag = true;
                } catch (const std::exception &e) {
                    last_error = e.what();
                }
            }(),
            ...);

        if (!flag && !last_error.empty()) {
            const auto mark = yaml_cpp_struct::detail::safe_mark(node);
            throw yaml_cpp_struct::Exception(yaml_cpp_struct::Error{
                yaml_cpp_struct::ErrorCode::InvalidField,
                {},
                yaml_cpp_struct::string_format("cannot decode as any variant alternative: %s", last_error.c_str()),
                mark.line,
                mark.column});
        }
        return flag;
    }

    static Node encode(const std::variant<T...> &rhs) {
        return std::visit([](const auto &v) { return yaml_cpp_struct::detail::to_node_impl(v); }, rhs);
    }
};

} // namespace YAML

// ---- preprocessor helpers for annotated fields ----
#define YCS_PP_EXPAND(x) x
#define YCS_PP_CONCAT_I(a, b) a##b
#define YCS_PP_CONCAT(a, b) YCS_PP_CONCAT_I(a, b)
#define YCS_PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21,   \
                     _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40,    \
                     _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59,    \
                     _60, _61, _62, _63, _64, N, ...)                                                                  \
    N
#define YCS_PP_NARG(...)                                                                                               \
    YCS_PP_EXPAND(YCS_PP_ARG_N(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47,    \
                               46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, \
                               24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,  \
                               0))

#define YCS_PP_MAP_STMT(m, ...) YCS_PP_EXPAND(YCS_PP_CONCAT(YCS_PP_MAP_STMT_, YCS_PP_NARG(__VA_ARGS__))(m, __VA_ARGS__))

#define YCS_PP_MAP_STMT_1(m, a1) m(a1)
#define YCS_PP_MAP_STMT_2(m, a1, ...) m(a1) YCS_PP_MAP_STMT_1(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_3(m, a1, ...) m(a1) YCS_PP_MAP_STMT_2(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_4(m, a1, ...) m(a1) YCS_PP_MAP_STMT_3(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_5(m, a1, ...) m(a1) YCS_PP_MAP_STMT_4(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_6(m, a1, ...) m(a1) YCS_PP_MAP_STMT_5(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_7(m, a1, ...) m(a1) YCS_PP_MAP_STMT_6(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_8(m, a1, ...) m(a1) YCS_PP_MAP_STMT_7(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_9(m, a1, ...) m(a1) YCS_PP_MAP_STMT_8(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_10(m, a1, ...) m(a1) YCS_PP_MAP_STMT_9(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_11(m, a1, ...) m(a1) YCS_PP_MAP_STMT_10(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_12(m, a1, ...) m(a1) YCS_PP_MAP_STMT_11(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_13(m, a1, ...) m(a1) YCS_PP_MAP_STMT_12(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_14(m, a1, ...) m(a1) YCS_PP_MAP_STMT_13(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_15(m, a1, ...) m(a1) YCS_PP_MAP_STMT_14(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_16(m, a1, ...) m(a1) YCS_PP_MAP_STMT_15(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_17(m, a1, ...) m(a1) YCS_PP_MAP_STMT_16(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_18(m, a1, ...) m(a1) YCS_PP_MAP_STMT_17(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_19(m, a1, ...) m(a1) YCS_PP_MAP_STMT_18(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_20(m, a1, ...) m(a1) YCS_PP_MAP_STMT_19(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_21(m, a1, ...) m(a1) YCS_PP_MAP_STMT_20(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_22(m, a1, ...) m(a1) YCS_PP_MAP_STMT_21(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_23(m, a1, ...) m(a1) YCS_PP_MAP_STMT_22(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_24(m, a1, ...) m(a1) YCS_PP_MAP_STMT_23(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_25(m, a1, ...) m(a1) YCS_PP_MAP_STMT_24(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_26(m, a1, ...) m(a1) YCS_PP_MAP_STMT_25(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_27(m, a1, ...) m(a1) YCS_PP_MAP_STMT_26(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_28(m, a1, ...) m(a1) YCS_PP_MAP_STMT_27(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_29(m, a1, ...) m(a1) YCS_PP_MAP_STMT_28(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_30(m, a1, ...) m(a1) YCS_PP_MAP_STMT_29(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_31(m, a1, ...) m(a1) YCS_PP_MAP_STMT_30(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_32(m, a1, ...) m(a1) YCS_PP_MAP_STMT_31(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_33(m, a1, ...) m(a1) YCS_PP_MAP_STMT_32(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_34(m, a1, ...) m(a1) YCS_PP_MAP_STMT_33(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_35(m, a1, ...) m(a1) YCS_PP_MAP_STMT_34(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_36(m, a1, ...) m(a1) YCS_PP_MAP_STMT_35(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_37(m, a1, ...) m(a1) YCS_PP_MAP_STMT_36(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_38(m, a1, ...) m(a1) YCS_PP_MAP_STMT_37(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_39(m, a1, ...) m(a1) YCS_PP_MAP_STMT_38(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_40(m, a1, ...) m(a1) YCS_PP_MAP_STMT_39(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_41(m, a1, ...) m(a1) YCS_PP_MAP_STMT_40(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_42(m, a1, ...) m(a1) YCS_PP_MAP_STMT_41(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_43(m, a1, ...) m(a1) YCS_PP_MAP_STMT_42(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_44(m, a1, ...) m(a1) YCS_PP_MAP_STMT_43(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_45(m, a1, ...) m(a1) YCS_PP_MAP_STMT_44(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_46(m, a1, ...) m(a1) YCS_PP_MAP_STMT_45(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_47(m, a1, ...) m(a1) YCS_PP_MAP_STMT_46(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_48(m, a1, ...) m(a1) YCS_PP_MAP_STMT_47(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_49(m, a1, ...) m(a1) YCS_PP_MAP_STMT_48(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_50(m, a1, ...) m(a1) YCS_PP_MAP_STMT_49(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_51(m, a1, ...) m(a1) YCS_PP_MAP_STMT_50(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_52(m, a1, ...) m(a1) YCS_PP_MAP_STMT_51(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_53(m, a1, ...) m(a1) YCS_PP_MAP_STMT_52(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_54(m, a1, ...) m(a1) YCS_PP_MAP_STMT_53(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_55(m, a1, ...) m(a1) YCS_PP_MAP_STMT_54(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_56(m, a1, ...) m(a1) YCS_PP_MAP_STMT_55(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_57(m, a1, ...) m(a1) YCS_PP_MAP_STMT_56(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_58(m, a1, ...) m(a1) YCS_PP_MAP_STMT_57(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_59(m, a1, ...) m(a1) YCS_PP_MAP_STMT_58(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_60(m, a1, ...) m(a1) YCS_PP_MAP_STMT_59(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_61(m, a1, ...) m(a1) YCS_PP_MAP_STMT_60(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_62(m, a1, ...) m(a1) YCS_PP_MAP_STMT_61(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_63(m, a1, ...) m(a1) YCS_PP_MAP_STMT_62(m, __VA_ARGS__)
#define YCS_PP_MAP_STMT_64(m, a1, ...) m(a1) YCS_PP_MAP_STMT_63(m, __VA_ARGS__)

#define YCS_PP_PROBE(x) x, 1,
#define YCS_PP_CHECK_N(x, n, ...) n
#define YCS_PP_CHECK(...) YCS_PP_CHECK_N(__VA_ARGS__, 0, )
#define YCS_PP_IS_PAREN_PROBE(...) YCS_PP_PROBE(~)
#define YCS_PP_IS_PAREN(x) YCS_PP_CHECK(YCS_PP_IS_PAREN_PROBE x)

// Bare field names are required.  YCS_OPT(field, default) adds a default.
// YCS_ENV(field, "ENV_NAME") binds a required field to an explicit env var.
// YCS_ENV_OPT(field, "ENV_NAME", default) adds a default and an explicit name.
#define YCS_OPT(field, ...) (opt, field, __VA_ARGS__)
#define YCS_ENV(field, env_name) (env, field, env_name)
#define YCS_ENV_OPT(field, env_name, ...) (env_opt, field, env_name, __VA_ARGS__)

#define YCS_DEFAULT_STMT(x) YCS_PP_CONCAT(YCS_DEFAULT_STMT_, YCS_PP_IS_PAREN(x))(x)
#define YCS_DEFAULT_STMT_0(x)
#define YCS_DEFAULT_STMT_1(x) YCS_DEFAULT_STMT_KIND x
#define YCS_DEFAULT_STMT_KIND(kind, field, ...) YCS_DEFAULT_STMT_KIND_##kind(field, __VA_ARGS__)
#define YCS_DEFAULT_STMT_KIND_opt(field, ...)                                                                          \
    if (std::string_view(name) == #field) {                                                                            \
        obj.field = __VA_ARGS__;                                                                                       \
        return true;                                                                                                   \
    }
#define YCS_DEFAULT_STMT_KIND_env(field, env_name)
#define YCS_DEFAULT_STMT_KIND_env_opt(field, env_name, ...)                                                            \
    if (std::string_view(name) == #field) {                                                                            \
        obj.field = __VA_ARGS__;                                                                                       \
        return true;                                                                                                   \
    }

#define YCS_ENV_NAME_STMT(x) YCS_PP_CONCAT(YCS_ENV_NAME_STMT_, YCS_PP_IS_PAREN(x))(x)
#define YCS_ENV_NAME_STMT_0(x)
#define YCS_ENV_NAME_STMT_1(x) YCS_ENV_NAME_STMT_KIND x
#define YCS_ENV_NAME_STMT_KIND(kind, field, ...) YCS_ENV_NAME_STMT_KIND_##kind(field, __VA_ARGS__)
#define YCS_ENV_NAME_STMT_KIND_opt(field, ...)
#define YCS_ENV_NAME_STMT_KIND_env(field, env_name)                                                                    \
    if (std::string_view(member_name) == #field)                                                                       \
        return env_name;
#define YCS_ENV_NAME_STMT_KIND_env_opt(field, env_name, ...)                                                           \
    if (std::string_view(member_name) == #field)                                                                       \
        return env_name;

#define YCS_VISIT_STMT(x) YCS_PP_CONCAT(YCS_VISIT_STMT_, YCS_PP_IS_PAREN(x))(x)
#define YCS_VISIT_STMT_0(x) f(#x, obj.x);
#define YCS_VISIT_STMT_1(x) YCS_VISIT_STMT_KIND x
#define YCS_VISIT_STMT_KIND(kind, field, ...) f(#field, obj.field);

#define YCS_VISIT_MEMBER_STMT(x) YCS_PP_CONCAT(YCS_VISIT_MEMBER_STMT_, YCS_PP_IS_PAREN(x))(x)
#define YCS_VISIT_MEMBER_STMT_0(x) ycs_f_(#x, x);
#define YCS_VISIT_MEMBER_STMT_1(x) YCS_VISIT_MEMBER_STMT_KIND x
#define YCS_VISIT_MEMBER_STMT_KIND(kind, field, ...) ycs_f_(#field, field);

#define YCS_DEFAULT_MEMBER_STMT(x) YCS_PP_CONCAT(YCS_DEFAULT_MEMBER_STMT_, YCS_PP_IS_PAREN(x))(x)
#define YCS_DEFAULT_MEMBER_STMT_0(x)
#define YCS_DEFAULT_MEMBER_STMT_1(x) YCS_DEFAULT_MEMBER_STMT_KIND x
#define YCS_DEFAULT_MEMBER_STMT_KIND(kind, field, ...) YCS_DEFAULT_MEMBER_STMT_KIND_##kind(field, __VA_ARGS__)
#define YCS_DEFAULT_MEMBER_STMT_KIND_opt(field, ...)                                                                   \
    if (std::string_view(ycs_name_) == #field) {                                                                       \
        field = __VA_ARGS__;                                                                                           \
        return true;                                                                                                   \
    }
#define YCS_DEFAULT_MEMBER_STMT_KIND_env(field, env_name)
#define YCS_DEFAULT_MEMBER_STMT_KIND_env_opt(field, env_name, ...)                                                     \
    if (std::string_view(ycs_name_) == #field) {                                                                       \
        field = __VA_ARGS__;                                                                                           \
        return true;                                                                                                   \
    }

// In-class equivalent of YCS_ADD_STRUCT.  Place it at the end of the struct,
// optionally in a private section.  The same field annotations are accepted:
//
//   struct Config {
//       std::string host;
//       int port;
//       bool debug;
//       YCS_DEFINE(host, port, YCS_OPT(debug, false))
//   };
#define YCS_DEFINE(...)                                                                                                \
    struct ycs_is_yaml_struct {};                                                                                      \
    template <typename YCS_Visitor> void ycs_visit(YCS_Visitor &&ycs_f_) {                                             \
        YCS_PP_MAP_STMT(YCS_VISIT_MEMBER_STMT, __VA_ARGS__)                                                            \
    }                                                                                                                  \
    template <typename YCS_Visitor> void ycs_visit(YCS_Visitor &&ycs_f_) const {                                       \
        YCS_PP_MAP_STMT(YCS_VISIT_MEMBER_STMT, __VA_ARGS__)                                                            \
    }                                                                                                                  \
    bool ycs_apply_default([[maybe_unused]] const char *ycs_name_) {                                                   \
        YCS_PP_MAP_STMT(YCS_DEFAULT_MEMBER_STMT, __VA_ARGS__)                                                          \
        return false;                                                                                                  \
    }                                                                                                                  \
    static const char *ycs_env_name([[maybe_unused]] const char *member_name) {                                        \
        YCS_PP_MAP_STMT(YCS_ENV_NAME_STMT, __VA_ARGS__)                                                                \
        return nullptr;                                                                                                \
    }                                                                                                                  \
    template <typename YCS_U> friend struct ::yaml_cpp_struct::detail::field_access;                                   \
    template <typename YCS_U> friend struct ::yaml_cpp_struct::detail::field_policies;                                 \
    template <typename, typename> friend struct ::yaml_cpp_struct::detail::has_ycs_define;

#define YCS_ADD_STRUCT(T, ...)                                                                                         \
    namespace YAML {                                                                                                   \
    template <> struct convert<T> {                                                                                    \
        template <typename U = T> static bool decode(const Node &node, T &rhs) {                                       \
            yaml_cpp_struct::detail::decode_struct<U>(node, rhs, #T);                                                  \
            return true;                                                                                               \
        }                                                                                                              \
        template <typename U = T> static Node encode(const T &rhs) {                                                   \
            return yaml_cpp_struct::detail::encode_struct<U>(rhs);                                                     \
        }                                                                                                              \
    };                                                                                                                 \
    }                                                                                                                  \
    namespace yaml_cpp_struct {                                                                                        \
    template <> inline constexpr bool is_yaml_struct<T> = true;                                                        \
    namespace detail {                                                                                                 \
    template <> struct struct_name<T> {                                                                                \
        static constexpr const char *value = #T;                                                                       \
    };                                                                                                                 \
    template <> struct field_access<T> {                                                                               \
        template <typename F> static void for_each(T &obj, F &&f) { YCS_PP_MAP_STMT(YCS_VISIT_STMT, __VA_ARGS__) }     \
        template <typename F> static void for_each(const T &obj, F &&f) {                                              \
            YCS_PP_MAP_STMT(YCS_VISIT_STMT, __VA_ARGS__)                                                               \
        }                                                                                                              \
    };                                                                                                                 \
    template <> struct field_policies<T> {                                                                             \
        static bool apply_default([[maybe_unused]] const char *name, [[maybe_unused]] T &obj) {                        \
            YCS_PP_MAP_STMT(YCS_DEFAULT_STMT, __VA_ARGS__)                                                             \
            return false;                                                                                              \
        }                                                                                                              \
        static const char *env_name([[maybe_unused]] const char *member_name) {                                        \
            YCS_PP_MAP_STMT(YCS_ENV_NAME_STMT, __VA_ARGS__)                                                            \
            return nullptr;                                                                                            \
        }                                                                                                              \
    };                                                                                                                 \
    }                                                                                                                  \
    }

#define YCS_ENV_PREFIX(T, prefix)                                                                                      \
    namespace yaml_cpp_struct {                                                                                        \
    template <> inline constexpr std::string_view env_prefix<T> = prefix;                                              \
    }

#define YCS_ADD_ENUM(E, ...)                                                                                           \
    namespace YAML {                                                                                                   \
    template <> struct convert<E> {                                                                                    \
        static bool decode(const Node &node, E &rhs) {                                                                 \
            if (!node.IsScalar()) {                                                                                    \
                const auto mark = yaml_cpp_struct::detail::safe_mark(node);                                            \
                throw yaml_cpp_struct::Exception(yaml_cpp_struct::Error{                                               \
                    yaml_cpp_struct::ErrorCode::BadEnum,                                                               \
                    {},                                                                                                \
                    yaml_cpp_struct::string_format("bad enum value for %s: expected a scalar", #E),                    \
                    mark.line,                                                                                         \
                    mark.column});                                                                                     \
            }                                                                                                          \
            const auto enum_str = node.Scalar();                                                                       \
            if (auto value = magic_enum::enum_cast<E>(enum_str); value.has_value()) {                                  \
                rhs = value.value();                                                                                   \
                return true;                                                                                           \
            }                                                                                                          \
            try {                                                                                                      \
                const auto raw = node.as<std::underlying_type_t<E>>();                                                 \
                if (auto value = magic_enum::enum_cast<E>(raw); value.has_value()) {                                   \
                    rhs = value.value();                                                                               \
                    return true;                                                                                       \
                }                                                                                                      \
            } catch (const std::exception &) {                                                                         \
            }                                                                                                          \
            const auto mark = yaml_cpp_struct::detail::safe_mark(node);                                                \
            throw yaml_cpp_struct::Exception(                                                                          \
                yaml_cpp_struct::Error{yaml_cpp_struct::ErrorCode::BadEnum,                                            \
                                       {},                                                                             \
                                       yaml_cpp_struct::string_format("bad enum value: %s", enum_str.c_str()),         \
                                       mark.line,                                                                      \
                                       mark.column});                                                                  \
        }                                                                                                              \
        static Node encode(const E &rhs) {                                                                             \
            const auto enum_name = magic_enum::enum_name(rhs);                                                         \
            if (enum_name.empty())                                                                                     \
                throw yaml_cpp_struct::Exception(yaml_cpp_struct::Error{                                               \
                    yaml_cpp_struct::ErrorCode::BadEnum,                                                               \
                    {},                                                                                                \
                    yaml_cpp_struct::string_format("bad enum value: %d", static_cast<int>(rhs)),                       \
                    -1,                                                                                                \
                    -1});                                                                                              \
            return Node(std::string(enum_name));                                                                       \
        }                                                                                                              \
    };                                                                                                                 \
    }

#endif
