#ifndef _YAML_CPP_STRUCT_HPP_
#define _YAML_CPP_STRUCT_HPP_

#include <chrono>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <yaml-cpp/yaml.h>
#include <visit_struct/visit_struct.hpp>
#include <tl/optional.hpp>

namespace yaml_cpp_struct {

template <typename... Args>
inline std::string string_format(const std::string& format, Args... args) {
	int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
	if (size_s <= 0)
		throw std::runtime_error("Error during formatting.");
	auto size = static_cast<size_t>(size_s);
	std::unique_ptr<char[]> buf(new char[size]());
	std::snprintf(buf.get(), size, format.c_str(), args...);
	return std::string(buf.get(), buf.get() + size - 1);
}

template <typename T>
inline T node_as(const YAML::Node& node) {
	return node.as<T>();
}

template <typename T>
inline auto cast(const T& v) -> T {
	return v;
}

inline auto cast(uint8_t v) -> uint32_t {
	return static_cast<uint32_t>(v);
}

template <typename T, T... Ints>
struct integer_sequence {
	typedef T value_type;
	static constexpr std::size_t size() { return sizeof...(Ints); }
};

template <std::size_t... Ints>
using index_sequence = integer_sequence<std::size_t, Ints...>;

template <typename T, std::size_t N, T... Is>
struct make_integer_sequence : make_integer_sequence<T, N - 1, N - 1, Is...> {};

template <typename T, T... Is>
struct make_integer_sequence<T, 0, Is...> : integer_sequence<T, Is...> {};

template <std::size_t N>
using make_index_sequence = make_integer_sequence<std::size_t, N>;

template <typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

template <typename... T, std::size_t... I>
inline std::tuple<T...> yaml_node_to_tuple(const YAML::Node& node, index_sequence<I...>) {
	return std::make_tuple(yaml_cpp_struct::node_as<T>(node[I])...);
}

template <typename N>
void process(N&&) {}

template <typename N, class T, class... Args>
void process(N&& node, T head, Args... rest) {
	node.push_back(yaml_cpp_struct::cast(head));
	process(node, rest...);
}

template <typename... T, std::size_t... I>
inline YAML::Node tuple_to_yaml_node(const std::tuple<T...>& tup, index_sequence<I...>) {
	YAML::Node node(YAML::NodeType::Sequence);
	process(node, std::get<I>(tup)...);
	return node;
}

template <typename T>
inline std::tuple<tl::optional<std::string>, std::string> to_yaml(const T& obj) {
	try {
		YAML::Emitter emitter;
		emitter << YAML::Node{yaml_cpp_struct::cast(obj)};
		return std::make_tuple(std::string{emitter.c_str()}, std::string{""});
	} catch (std::exception& e) {
		return std::make_tuple(tl::nullopt, string_format("Emitter to string: %s", e.what()));
	}
}

template <typename T>
inline std::tuple<tl::optional<T>, std::string> from_yaml(const std::string& filename) {
	try {
		const auto node = YAML::LoadFile(filename);
		return std::make_tuple(yaml_cpp_struct::node_as<T>(node), "");
	} catch (const YAML::BadFile& e) {
		return std::make_tuple(tl::nullopt, string_format("%s not found or broken", filename.c_str()));
	} catch (const std::exception& e) {
		return std::make_tuple(tl::nullopt, string_format("on parsing %s: %s", filename.c_str(), e.what()));
	}
}

} // namespace yaml_cpp_struct

namespace YAML {

template <typename K, typename V>
struct convert<std::unordered_map<K, V>> {
	static bool decode(const Node& node, std::unordered_map<K, V>& rhs) {
		if (!node.IsMap())
			return false;
		rhs.clear();
		for (auto& it : node)
			rhs.emplace(it.first.template as<K>(), it.second.template as<V>());
		return true;
	}

	static Node encode(const std::unordered_map<K, V>& rhs) {
		Node node(NodeType::Map);
		for (auto& value : rhs)
			node.force_insert(yaml_cpp_struct::cast(value.first), yaml_cpp_struct::cast(value.second));
		return node;
	}
};

template <typename R, typename P>
struct convert<std::chrono::duration<R, P>> {
	static bool decode(const Node& node, std::chrono::duration<R, P>& rhs) {
		if (!node.IsScalar())
			return false;
		rhs = std::chrono::duration<R, P>(node.template as<R>());
		return true;
	}

	static Node encode(const std::chrono::duration<R, P>& rhs) {
		return Node(yaml_cpp_struct::cast(rhs.count()));
	}
};

template <typename V>
struct convert<std::unordered_set<V>> {
	static bool decode(const Node& node, std::unordered_set<V>& rhs) {
		if (!node.IsSequence())
			return false;
		rhs.clear();
		for (auto& it : node)
			rhs.emplace(it.template as<V>());
		return true;
	}

	static Node encode(const std::unordered_set<V>& rhs) {
		Node node(NodeType::Sequence);
		for (auto& it : rhs)
			node.push_back(Node(yaml_cpp_struct::cast(it)));
		return node;
	}
};

template <typename T>
struct convert<tl::optional<T>> {
	static bool decode(const Node& node, tl::optional<T>& rhs) {
		if (node.IsNull()) {
			rhs = tl::nullopt;
		}
		else
			rhs = yaml_cpp_struct::node_as<T>(node);
		return true;
	}

	static Node encode(const tl::optional<T>& rhs) {
		if (rhs.has_value())
			return Node(yaml_cpp_struct::cast(rhs.value()));
		else
			return Node{};
	}
};

template <typename... T>
struct convert<std::tuple<T...>> {
	static bool decode(const Node& node, std::tuple<T...>& rhs) {
		if (!node.IsSequence() || (node.size() != sizeof...(T)))
			return false;
		rhs = yaml_cpp_struct::yaml_node_to_tuple<T...>(node, yaml_cpp_struct::index_sequence_for<T...>{});
		return true;
	}

	static Node encode(const std::tuple<T...>& rhs) {
		return yaml_cpp_struct::tuple_to_yaml_node<T...>(rhs, yaml_cpp_struct::index_sequence_for<T...>{});
	}
};

} // namespace YAML

namespace yaml_cpp_struct {

template <typename N>
struct Cpp11EncodeHelper {
	template <typename T>
	void operator()(const char* name, const T& value) {
		node[name] = value;
	}
	N& node;
};

template <typename N>
struct Cpp11DecodeHelper {
	template <typename T>
	void operator()(const char* name, T& value) {
		try {
			value = yaml_cpp_struct::node_as<T>(node[name]);
		} catch (const std::runtime_error&) {
		}
	}
	const N& node;
};

} // namespace yaml_cpp_struct

#define YCS_ADD_STRUCT(T, ...)                                                           \
	VISITABLE_STRUCT(T, __VA_ARGS__);                                                    \
	namespace YAML {                                                                     \
	template <>                                                                          \
	struct convert<T> {                                                                  \
		static bool decode(const Node& node, T& rhs) {                                   \
			visit_struct::for_each(rhs, yaml_cpp_struct::Cpp11DecodeHelper<Node>{node}); \
			return true;                                                                 \
		}                                                                                \
		static Node encode(const T& rhs) {                                               \
			Node node(NodeType::Map);                                                    \
			visit_struct::for_each(rhs, yaml_cpp_struct::Cpp11EncodeHelper<Node>{node}); \
			return node;                                                                 \
		}                                                                                \
	};                                                                                   \
	}

#define YCS_ADD_ENUM(E, decode_func, encode_func)      \
	namespace YAML {                                   \
	template <>                                        \
	struct convert<E> {                                \
		static bool decode(const Node& node, E& rhs) { \
			auto enum_str = node.as<std::string>();    \
			rhs = decode_func(enum_str);               \
			return true;                               \
		}                                              \
		static Node encode(const E& rhs) {             \
			return Node{encode_func(rhs)};             \
		}                                              \
	};                                                 \
	}

#endif
