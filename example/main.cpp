#include <iostream>

#include <spdlog/spdlog.h>
#include <yaml_cpp_struct.hpp>

enum class AccountType : uint16_t {
	Personal = 1,
	Company = 2,
};
YCS_ADD_ENUM(
	AccountType,
	[](std::string name) {
		if (name == "Personal")
			return AccountType::Personal;
		return AccountType::Company;
	},
	[](AccountType type) {
		if (type == AccountType::Personal)
			return "Personal";
		return "Company";
	})

struct AccountInfo {
	bool flag;
	std::string name;
	std::unordered_map<std::string, int> address;
	tl::optional<std::string> num{tl::nullopt};
	std::chrono::milliseconds msec;
	std::tuple<std::string, uint8_t> tuple;
	std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple;
};
YCS_ADD_STRUCT(AccountInfo, flag, name, address, num, msec, tuple, map_tuple)

struct DefaultTest {
	bool flag{false};
	std::string name{"hello world"};
	std::unordered_map<std::string, int> address{{"127.0.0.1", 8856}};
	tl::optional<std::string> num{"90"};
	std::chrono::milliseconds msec{56};
	std::tuple<std::string, uint8_t> tuple{std::make_tuple("tuple", 58)};
	std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple{{"test-default", std::make_tuple("001", 23)}};
	AccountType account_type{AccountType::Company};
};
YCS_ADD_STRUCT(DefaultTest, flag, name, address, num, msec, tuple, map_tuple, account_type)

struct Config {
	char ch;
	double price;
	int16_t count;
	std::string content;
	std::unordered_map<std::string, std::string> map;
	AccountInfo account_info;
	std::vector<std::string> vec;
	std::unordered_set<uint8_t> set_vec;
	DefaultTest default_test;
	std::string default_str{"hello default"};
	tl::optional<std::string> default_opt{};
	AccountType account_type{AccountType::Company};
};
YCS_ADD_STRUCT(Config, ch, price, count, content, map, account_info, vec, set_vec, default_test, default_str, default_opt, account_type)

int main(int, char** argv) {
	auto value = yaml_cpp_struct::from_yaml<Config>(argv[1]);
	auto& cfg = std::get<0>(value);
	auto& error = std::get<1>(value);
	if (!cfg) {
		spdlog::error("{}", error);
		return -1;
	}
	auto ret = yaml_cpp_struct::to_yaml(cfg.value());
	spdlog::info("\n{}", std::get<0>(ret).value());
	return 0;
}
