// #define OPEN_YAML_TO_JSON

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <spdlog/spdlog.h>
#include <yaml_cpp_struct.hpp>

namespace {

bool g_checks_ok = true;

void check(bool condition, const std::string &message) {
    if (!condition) {
        spdlog::error("check failed: {}", message);
        g_checks_ok = false;
    }
}

void set_env(const char *name, const char *value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    ::setenv(name, value, 1);
#endif
}

} // namespace

template <typename... Ts> std::string to_string(const Ts &...ts) {
    std::stringstream ss;
    const char *sep = "";
    auto func = [](auto &t) {
        if constexpr (std::is_same_v<std::decay_t<decltype(t)>, int8_t> ||
                      std::is_same_v<std::decay_t<decltype(t)>, uint8_t>)
            return static_cast<int32_t>(t);
        else
            return t;
    };
    ((static_cast<void>(ss << sep << func(ts)), sep = " "), ...);
    return ss.str();
}

template <typename... Args> std::string to_string(const std::tuple<Args...> &t) {
    return std::apply([](const auto &...ts) { return to_string(ts...); }, t);
}

template <typename T>
inline std::enable_if_t<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value, std::string> to_string(T value) {
    return std::to_string(value);
}

inline std::string to_string(bool value) { return value ? "true" : "false"; }

inline std::string to_string(const std::string &value) { return value; }

template <typename T> inline std::string to_string(const std::vector<T> &vector) {
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

template <typename T> inline std::string to_string(const std::unordered_set<T> &vector) {
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

template <typename K, typename V> inline std::string to_string(const std::unordered_map<K, V> &map) {
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

template <typename T> inline std::string to_string(const std::optional<T> &opt) {
    std::stringstream ss;
    if (opt) {
        if constexpr (std::is_fundamental_v<T> || std::is_same_v<T, std::string>)
            ss << opt.value();
        else
            ss << to_string(opt.value());
    } else
        ss << "(nullopt)";
    return ss.str();
}

enum class AccountType : uint8_t {
    Personal = 1,
    Company = 2,
};
YCS_ADD_ENUM(AccountType, Personal, Company)

std::string to_string(const AccountType &type) {
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
YCS_ADD_STRUCT(AccountInfo, flag, name, address, YCS_OPT(num, std::nullopt), msec, tuple, map_tuple)

std::string to_string(const AccountInfo &aif) {
    std::stringstream ss;
    ss << "AccountInfo"
       << " name=" << aif.name << " address=" << to_string(aif.address) << " num=" << to_string(aif.num)
       << " msec=" << aif.msec.count() << " tuple=" << to_string(aif.tuple) << " map_tuple=" << to_string(aif.map_tuple)
       << " flag=" << to_string(aif.flag);
    return ss.str();
}

struct DefaultTest {
    bool flag{false};
    std::string name{"hello world"};
    std::unordered_map<std::string, int> address{{"127.0.0.1", 8856}};
    std::optional<std::string> num{"90"};
    std::chrono::milliseconds msec{56};
    std::tuple<std::string, uint8_t> tuple{std::make_tuple("tuple", 58)};
    std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple{
        {"test-default", std::make_tuple("001", 23)}};
    AccountType account_type{AccountType::Company};
};
YCS_ADD_STRUCT(DefaultTest, YCS_OPT(flag, false), YCS_OPT(name, "hello world"),
               YCS_OPT(address, (std::unordered_map<std::string, int>{{"127.0.0.1", 8856}})),
               YCS_OPT(num, std::string{"90"}), YCS_OPT(msec, std::chrono::milliseconds{56}),
               YCS_OPT(tuple, (std::make_tuple("tuple", uint8_t{58}))),
               YCS_OPT(map_tuple, (std::unordered_map<std::string, std::tuple<std::string, int32_t>>{
                                      {"test-default", std::make_tuple("001", 23)}})),
               YCS_OPT(account_type, AccountType::Company))

std::string to_string(const DefaultTest &aif) {
    std::stringstream ss;
    ss << "DefaultTest"
       << " name=" << aif.name << " address=" << to_string(aif.address) << " num=" << to_string(aif.num)
       << " msec=" << aif.msec.count() << " tuple=" << to_string(aif.tuple) << " map_tuple=" << to_string(aif.map_tuple)
       << " flag=" << to_string(aif.flag);
    return ss.str();
}

// Exercises the 8-bit integer handling.  yaml-cpp streams char types as
// characters, so the wrapper has to encode them as numbers explicitly.
struct ByteFields {
    uint8_t byte{0};
    int8_t sbyte{0};
};
YCS_ADD_STRUCT(ByteFields, byte, sbyte)

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
    std::variant<int, std::vector<int>> v1;
    DefaultTest default_test;
    std::string default_str{"hello default"};
    std::optional<std::string> default_opt{std::nullopt};
    std::vector<std::string> ips;
};
YCS_ADD_STRUCT(Config, ch, price, count, content, map, account_info, vec, set_vec, account_type, v1,
               YCS_OPT(default_test, DefaultTest{}), YCS_OPT(default_str, "hello default"),
               YCS_OPT(default_opt, std::nullopt), ips)
YCS_ENV_PREFIX(Config, "YCS_ENV_")

std::string to_string(const Config &cfg) {
    std::stringstream ss;
    ss << "Config"
       << " price=" << std::to_string(cfg.price) << " count=" << (int32_t)cfg.count << " content=" << cfg.content
       << " map=" << to_string(cfg.map) << " account_info=" << to_string(cfg.account_info)
       << " vec=" << to_string(cfg.vec) << " set_vec=" << to_string(cfg.set_vec)
       << " account_type=" << to_string(cfg.account_type) << " ch=" << to_string(cfg.ch)
       << " v1=" << to_string(std::get<1>(cfg.v1)) << " default_test=" << to_string(cfg.default_test)
       << " default_str=" << to_string(cfg.default_str) << " default_opt=" << to_string(cfg.default_opt)
       << " ips=" << to_string(cfg.ips);
    return ss.str();
}

void verify_config(const Config &cfg) {
    check(cfg.ch == 'A', "ch");
    check(cfg.price > 100.049 && cfg.price < 100.051, "price");
    check(cfg.count == 254, "count");
    check(cfg.content == "i'm fantasy-peak", "content");

    const auto fantasy = cfg.map.find("fantasy");
    const auto peak = cfg.map.find("peak");
    check(fantasy != cfg.map.end() && fantasy->second == "good", "map.fantasy");
    check(peak != cfg.map.end() && peak->second == "good", "map.peak");

    check(cfg.account_info.flag == false, "account_info.flag");
    check(cfg.account_info.name == "fantasy-peak", "account_info.name");

    const auto addr1 = cfg.account_info.address.find("127.0.0.1");
    const auto addr2 = cfg.account_info.address.find("127.0.0.2");
    check(addr1 != cfg.account_info.address.end() && addr1->second == 8888, "account_info.address[127.0.0.1]");
    check(addr2 != cfg.account_info.address.end() && addr2->second == 9999, "account_info.address[127.0.0.2]");

    check(!cfg.account_info.num.has_value(), "account_info.num should be nullopt");
    check(cfg.account_info.msec.count() == 100, "account_info.msec");
    check(std::get<0>(cfg.account_info.tuple) == "fantasy-peak", "account_info.tuple[0]");
    check(std::get<1>(cfg.account_info.tuple) == 89, "account_info.tuple[1]");

    const auto map_tuple = cfg.account_info.map_tuple.find("fantasy-peak");
    check(map_tuple != cfg.account_info.map_tuple.end(), "account_info.map_tuple key");
    if (map_tuple != cfg.account_info.map_tuple.end()) {
        check(std::get<0>(map_tuple->second) == "map_tuple", "account_info.map_tuple[0]");
        check(std::get<1>(map_tuple->second) == 254, "account_info.map_tuple[1]");
    }

    check(cfg.vec == std::vector<std::string>{"fantasy-001", "fantasy-002"}, "vec");
    check(cfg.set_vec == std::unordered_set<uint8_t>{9, 5, 7}, "set_vec");
    check(cfg.account_type == AccountType::Personal, "account_type");
    check(std::holds_alternative<std::vector<int>>(cfg.v1), "v1 alternative");
    if (std::holds_alternative<std::vector<int>>(cfg.v1))
        check(std::get<std::vector<int>>(cfg.v1) == std::vector<int>{8, 5, 65}, "v1 value");

    // default_test is absent from config.yaml, so every field keeps its
    // in-class initializer.
    check(cfg.default_test.flag == false, "default_test.flag");
    check(cfg.default_test.name == "hello world", "default_test.name");
    check(cfg.default_test.num.has_value() && *cfg.default_test.num == "90", "default_test.num");
    check(cfg.default_test.msec.count() == 56, "default_test.msec");
    check(std::get<0>(cfg.default_test.tuple) == "tuple", "default_test.tuple[0]");
    check(std::get<1>(cfg.default_test.tuple) == 58, "default_test.tuple[1]");
    check(cfg.default_test.account_type == AccountType::Company, "default_test.account_type");
    check(cfg.default_test.map_tuple.size() == 1, "default_test.map_tuple size");

    check(cfg.default_str == "hello default", "default_str");
    check(cfg.default_opt.has_value() && *cfg.default_opt == "yyds", "default_opt");
    if (std::getenv("YCS_ENV_IPS") == nullptr)
        check(cfg.ips.empty(), "ips should be empty without an environment override");
}

void verify_round_trip(const Config &cfg) {
    const auto yaml_result = yaml_cpp_struct::to_yaml(cfg);
    check(yaml_result.value.has_value(), "to_yaml should succeed");
    check(yaml_result.error.code == yaml_cpp_struct::ErrorCode::None, "to_yaml error message");
    if (!yaml_result)
        return;

    const auto round_result = yaml_cpp_struct::from_yaml_str<Config>(*yaml_result);
    check(round_result.value.has_value(), "round-trip should parse");
    check(round_result.error.code == yaml_cpp_struct::ErrorCode::None, "round-trip error message");
    if (!round_result)
        return;

    const Config &round_trip = *round_result;
    check(round_trip.account_info.name == cfg.account_info.name, "round-trip account_info.name");
    check(round_trip.set_vec == cfg.set_vec, "round-trip set_vec");
    check(round_trip.account_type == cfg.account_type, "round-trip account_type");
    check(round_trip.v1 == cfg.v1, "round-trip v1");
    check(round_trip.default_opt == cfg.default_opt, "round-trip default_opt");
}

void verify_8bit_fields() {
    const auto bytes_result = yaml_cpp_struct::from_yaml_str<ByteFields>("byte: 200\nsbyte: -100\n");
    check(bytes_result.value.has_value(), "8-bit fields should parse");
    check(bytes_result.error.code == yaml_cpp_struct::ErrorCode::None, "8-bit parse error message");
    if (!bytes_result)
        return;

    const ByteFields &bytes = *bytes_result;
    check(bytes.byte == 200, "uint8_t field");
    check(bytes.sbyte == -100, "int8_t field");

    const auto encoded_result = yaml_cpp_struct::to_yaml(bytes);
    check(encoded_result.value.has_value(), "8-bit fields should encode");
    check(encoded_result.error.code == yaml_cpp_struct::ErrorCode::None, "8-bit emit error message");
    if (encoded_result) {
        check(encoded_result->find("200") != std::string::npos, "uint8_t should encode as a number");
        check(encoded_result->find("-100") != std::string::npos, "int8_t should encode as a number");
    }

    const auto overflow = yaml_cpp_struct::from_yaml_str<ByteFields>("byte: 300\nsbyte: 0\n");
    check(!overflow.value.has_value(), "uint8_t overflow should be rejected");
    check(overflow.error.code == yaml_cpp_struct::ErrorCode::OutOfRange, "uint8_t overflow error code");
}

void verify_set_container() {
    const auto values_result = yaml_cpp_struct::from_yaml_str<std::set<int>>("[3, 1, 2, 2]\n");
    check(values_result.value.has_value(), "std::set should parse");
    check(values_result.error.code == yaml_cpp_struct::ErrorCode::None, "std::set parse error message");
    if (!values_result)
        return;

    const std::set<int> &values = *values_result;
    check(values == std::set<int>{1, 2, 3}, "std::set contents");

    const auto encoded_result = yaml_cpp_struct::to_yaml(values);
    check(encoded_result.value.has_value(), "std::set should encode");
    check(encoded_result.error.code == yaml_cpp_struct::ErrorCode::None, "std::set emit error message");
    if (encoded_result)
        check(encoded_result->find("1") != std::string::npos && encoded_result->find("3") != std::string::npos,
              "std::set encoding");
}

void verify_enum_values() {
    const auto by_name = yaml_cpp_struct::from_yaml_str<AccountType>("\"Company\"\n");
    check(by_name.value.has_value() && *by_name.value == AccountType::Company, "enum decoded by name");
    check(by_name.error.code == yaml_cpp_struct::ErrorCode::None, "enum by-name error message");

    const auto by_value = yaml_cpp_struct::from_yaml_str<AccountType>("1\n");
    check(by_value.value.has_value() && *by_value.value == AccountType::Personal, "enum decoded by numeric value");
    check(by_value.error.code == yaml_cpp_struct::ErrorCode::None, "enum by-value error message");

    const auto invalid = yaml_cpp_struct::from_yaml_str<AccountType>("\"Unknown\"\n");
    check(!invalid.value.has_value(), "unknown enum name should be rejected");
    check(invalid.error.code == yaml_cpp_struct::ErrorCode::BadEnum, "unknown enum error code");
}

void verify_env_overrides(const char *config_path) {
    // String fields use the raw env value; no YAML quoting is needed.
    set_env("YCS_ENV_ACCOUNT_INFO__NAME", "from-env");
    set_env("YCS_ENV_IPS", "[\"Google\", \"Runoob\", \"Taobao\"]");

    const auto cfg_result = yaml_cpp_struct::from_yaml<Config>(config_path);
    check(cfg_result.value.has_value(), "environment config should parse");
    check(cfg_result.error.code == yaml_cpp_struct::ErrorCode::None, "environment error message");
    if (!cfg_result)
        return;

    const Config &cfg = *cfg_result;
    check(cfg.account_info.name == "from-env", "nested environment override");
    check(cfg.ips == std::vector<std::string>{"Google", "Runoob", "Taobao"}, "top-level environment override");
}

int main(int, char **argv) {
    spdlog::info("Load yaml config");
    const auto cfg_result = yaml_cpp_struct::from_yaml<Config>(argv[1]);
    if (!cfg_result) {
        spdlog::error("error: [{}]", yaml_cpp_struct::format_error(cfg_result.error));
        return -1;
    }
    const Config &cfg = *cfg_result;

    spdlog::info("\n{}", to_string(cfg));

    const auto yaml_result = yaml_cpp_struct::to_yaml(cfg);
    if (!yaml_result) {
        spdlog::error("to_yaml error: [{}]", yaml_cpp_struct::format_error(yaml_result.error));
        return -1;
    }
    spdlog::info("\n{}", *yaml_result);

#ifdef OPEN_YAML_TO_JSON
    spdlog::info("\n{}", yaml_cpp_struct::yaml_to_json(*yaml_result).dump());
#endif

    verify_config(cfg);
    verify_round_trip(cfg);
    verify_8bit_fields();
    verify_set_container();
    verify_enum_values();
    verify_env_overrides(argv[1]);

    if (!g_checks_ok) {
        spdlog::error("example self-checks failed");
        return -1;
    }

    spdlog::info("all self-checks passed");
    return 0;
}
