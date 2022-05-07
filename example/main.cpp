#include <spdlog/spdlog.h>
#include <yaml_cpp_struct.hpp>

template <typename... Ts>
std::string to_string(const Ts&... ts) {
	std::stringstream ss;
	const char* sep = "";
	auto func = [](auto& t) {
		if constexpr (std::is_same_v<std::decay_t<decltype(t)>, int8_t> || std::is_same_v<std::decay_t<decltype(t)>, uint8_t>)
			return static_cast<int32_t>(t);
		else
			return t;
	};
	((static_cast<void>(ss << sep << func(ts)), sep = " "), ...);
	return ss.str();
}

template <typename... Args>
std::string to_string(const std::tuple<Args...>& t) {
	return std::apply([](const auto&... ts) { return to_string(ts...); }, t);
}

template <typename T>
inline std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, std::string> to_string(T value) {
	return std::to_string(value);
}

inline std::string to_string(bool value) {
	return value ? "true" : "false";
}

inline std::string to_string(const std::string& value) {
	return value;
}

template <typename T>
inline std::string to_string(const std::vector<T>& vector) {
	std::string str = "[";
	auto it = vector.begin();
	if (it != vector.end()) {
		str += to_string(*it);
		++it;
	}
	for (; it != vector.end(); ++it) {
		str += ",";
		str += to_string(*it);
	}
	str += "]";
	return str;
}

template <typename T>
inline std::string to_string(const std::unordered_set<T>& vector) {
	std::string str = "[";
	auto it = vector.begin();
	if (it != vector.end()) {
		str += to_string(*it);
		++it;
	}
	for (; it != vector.end(); ++it) {
		str += ",";
		str += to_string(*it);
	}
	str += "]";
	return str;
}

template <typename K, typename V>
inline std::string to_string(const std::unordered_map<K, V>& map) {
	std::string str = "{";
	auto it = map.begin();
	if (it != map.end()) {
		str += to_string(it->first);
		str += "->";
		str += to_string(it->second);
		++it;
	}
	for (; it != map.end(); ++it) {
		str += ",";
		str += to_string(it->first);
		str += "->";
		str += to_string(it->second);
	}
	str += "}";
	return str;
}

template <typename T>
inline std::string to_string(const std::optional<T>& opt) {
	std::stringstream ss;
	if (opt) {
		if constexpr (std::is_fundamental_v<T> || std::is_same_v<T, std::string>)
			ss << opt.value();
		else
			ss << to_string(opt.value());
	}
	else
		ss << "(nullopt)";
	return ss.str();
}

enum class AccountType : uint8_t {
	Personal = 1,
	Company = 2,
};
YCS_ADD_ENUM(AccountType, Personal, Company)

std::string to_string(const AccountType& type) {
	if (type == AccountType::Personal)
		return "Personal";
	if (type == AccountType::Company)
		return "Company";
	throw std::bad_cast();
}

struct AccountInfo {
	bool flag;
	std::string name;
	std::unordered_map<std::string, int> address;
	std::optional<std::string> num;
	std::chrono::milliseconds msec;
	std::tuple<std::string, uint8_t> tuple;
	std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple;
};
YCS_ADD_STRUCT(AccountInfo, flag, name, address, num, msec, tuple, map_tuple)

std::string to_string(const AccountInfo& aif) {
	std::stringstream ss;
	ss << "AccountInfo"
	   << " name=" << aif.name << " address=" << to_string(aif.address) << " num=" << to_string(aif.num)
	   << " msec=" << aif.msec.count() << " tuple=" << to_string(aif.tuple) << " map_tuple=" << to_string(aif.map_tuple)
	   << " flag=" << to_string(aif.flag);
	return ss.str();
}

struct Config {
	char ch;
	double price;
	int16_t count;
	std::string content;
	std::unordered_map<std::string, std::string> map;
	AccountInfo account_info;
	std::vector<std::string> vec;
	std::unordered_set<uint8_t> set_vec;
	AccountType account_type;
};
YCS_ADD_STRUCT(Config, ch, price, count, content, map, account_info, vec, set_vec, account_type)

std::string to_string(const Config& cfg) {
	std::stringstream ss;
	ss << "Config"
	   << " price=" << std::to_string(cfg.price) << " count=" << (int32_t)cfg.count
	   << " content=" << cfg.content << " map=" << to_string(cfg.map) << " account_info=" << to_string(cfg.account_info)
	   << " vec=" << to_string(cfg.vec) << " set_vec=" << to_string(cfg.set_vec) << " account_type=" << to_string(cfg.account_type)
	   << " ch=" << to_string(cfg.ch);
	return ss.str();
}

int main(int, char** argv) {
	auto [cfg, error] = yaml_cpp_struct::from_yaml<Config>(argv[1]);
	if (!cfg) {
		spdlog::error("{}", error);
		return -1;
	}
	spdlog::info("\n{}", to_string(cfg.value()));
	auto [str, e] = yaml_cpp_struct::to_yaml(cfg.value());
	spdlog::info("\n{}", str.value());
	return 0;
}
