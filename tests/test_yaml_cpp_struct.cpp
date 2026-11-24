#include <gtest/gtest.h>

#include <nlohmann/json.hpp>
#include <yaml_cpp_struct.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

namespace {

template <typename T> auto parse(const std::string &yaml) { return yaml_cpp_struct::from_yaml<T, false>(yaml); }

yaml_cpp_struct::EnvLookup make_env(std::map<std::string, std::string, std::less<>> values) {
    return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
        const auto it = values.find(std::string(name));
        if (it == values.end())
            return std::nullopt;
        return it->second;
    };
}

} // namespace

enum class Color : uint8_t {
    Red = 1,
    Green = 2,
    Blue = 3,
};
YCS_ADD_ENUM(Color, Red, Green, Blue)

struct Inner {
    int value{7};
    std::string label{"inner"};
};
YCS_ADD_STRUCT(Inner, value, label)

struct Everything {
    char ch{'x'};
    double price{0.0};
    int16_t count{0};
    std::string text{"default"};
    std::vector<std::string> vec;
    std::unordered_set<uint8_t> u8set;
    std::set<int> iset;
    std::unordered_map<std::string, int> umap;
    std::array<int, 2> arr{{0, 0}};
    std::list<int> list;
    std::pair<int, std::string> pair{0, ""};
    std::tuple<std::string, uint8_t> tup{};
    std::optional<std::string> opt{std::nullopt};
    std::chrono::milliseconds msec{0};
    std::variant<int, std::vector<int>> var{0};
    Color color{Color::Red};
    Inner inner;
};
YCS_ADD_STRUCT(Everything, ch, price, count, text, vec, u8set, iset, umap, arr, list, pair, tup,
               YCS_OPT(opt, std::nullopt), msec, var, color, inner)

struct ByteFields {
    uint8_t byte{0};
    int8_t sbyte{0};
};
YCS_ADD_STRUCT(ByteFields, byte, sbyte)

struct OptionalFields {
    int value{0};
    std::optional<int> opt;
};
YCS_ADD_STRUCT(OptionalFields, value, YCS_OPT(opt, std::nullopt))

struct EnvConfig {
    std::string name{"default"};
    Inner nested;
};
YCS_ADD_STRUCT(EnvConfig, name, nested)
YCS_ENV_PREFIX(EnvConfig, "APP_")

struct EnvOnly {
    int required_field{0};
    std::string optional_field{"default"};
};
YCS_ADD_STRUCT(EnvOnly, required_field, YCS_OPT(optional_field, "default"))
YCS_ENV_PREFIX(EnvOnly, "APP_")

struct ExplicitEnv {
    int port{0};
    int timeout{0};
};
YCS_ADD_STRUCT(ExplicitEnv, YCS_ENV(port, "CUSTOM_PORT"), YCS_ENV_OPT(timeout, "CUSTOM_TIMEOUT", 30))

struct ExplicitNested {
    Inner inner;
};
YCS_ADD_STRUCT(ExplicitNested, YCS_ENV(inner, "CUSTOM_INNER"))

struct EnvLists {
    std::vector<int> values;
};
YCS_ADD_STRUCT(EnvLists, values)
YCS_ENV_PREFIX(EnvLists, "APP_")

struct NoPrefix {
    int port{7};
};
YCS_ADD_STRUCT(NoPrefix, YCS_OPT(port, 7))

struct DuplicateEnv {
    int a{0};
    int b{0};
};
YCS_ADD_STRUCT(DuplicateEnv, YCS_ENV(a, "SAME_ENV"), YCS_ENV(b, "SAME_ENV"))

// In-class initializers deliberately differ from the YCS_OPT defaults, so the
// tests prove that YCS_OPT is the single source of truth for missing fields.
struct DefaultsOnly {
    int a{99};
    std::string b{"in-class"};
    std::optional<int> c;
};
YCS_ADD_STRUCT(DefaultsOnly, YCS_OPT(a, 0), YCS_OPT(b, "default"), YCS_OPT(c, std::nullopt))

struct MixedRequired {
    int required_field{0};
    std::string optional_field{"in-class"};
};
YCS_ADD_STRUCT(MixedRequired, required_field, YCS_OPT(optional_field, "default"))

struct TextOnly {
    std::string text;
};
YCS_ADD_STRUCT(TextOnly, text)

struct NestedOnly {
    Inner inner;
};
YCS_ADD_STRUCT(NestedOnly, inner)

struct IntHash {
    std::size_t operator()(int value) const noexcept { return std::hash<int>{}(value); }
};

struct Scalars {
    bool boolean{false};
    float f{0.0f};
    double d{0.0};
    long double ld{0.0L};
    int8_t i8{0};
    uint8_t u8{0};
    int16_t i16{0};
    uint16_t u16{0};
    int32_t i32{0};
    uint32_t u32{0};
    int64_t i64{0};
    uint64_t u64{0};
    short s{0};
    unsigned short us{0};
    long l{0};
    unsigned long ul{0};
    long long ll{0};
    unsigned long long ull{0};
};
YCS_ADD_STRUCT(Scalars, boolean, f, d, ld, i8, u8, i16, u16, i32, u32, i64, u64, s, us, l, ul, ll, ull)

struct Maps {
    std::map<std::string, int> ordered;
    std::map<int, std::string, std::greater<int>> descending;
    std::unordered_map<int, int, IntHash> hashed;
};
YCS_ADD_STRUCT(Maps, ordered, descending, hashed)

// Default expressions deliberately contain commas to exercise the variadic
// YCS_OPT capture.
struct ComplexDefaults {
    std::unordered_map<std::string, int> map;
    std::tuple<int, std::string> tup;
    std::vector<int> vec;
};
YCS_ADD_STRUCT(ComplexDefaults, YCS_OPT(map, (std::unordered_map<std::string, int>{{"a", 1}})),
               YCS_OPT(tup, (std::make_tuple(1, std::string("x")))), YCS_OPT(vec, (std::vector<int>{1, 2, 3})))

struct OptionalContainers {
    std::optional<std::vector<int>> opt_vec;
    std::optional<std::map<std::string, int>> opt_map;
};
YCS_ADD_STRUCT(OptionalContainers, YCS_OPT(opt_vec, std::nullopt), YCS_OPT(opt_map, std::nullopt))

struct NestedContainers {
    std::map<std::string, std::vector<int>> groups;
    std::vector<std::optional<int>> maybe_values;
};
YCS_ADD_STRUCT(NestedContainers, groups, maybe_values)

namespace {

const char *kEverythingYaml = R"(
ch: z
price: 12.5
count: 42
text: hello
vec: [a, b]
u8set: [9, 5, 7]
iset: [3, 1, 2]
umap: {one: 1, two: 2}
arr: [10, 20]
list: [4, 5]
pair: [1, one]
tup: [tuple, 89]
opt: value
msec: 1500
var: [8, 5, 65]
color: Green
inner:
  value: 99
  label: nested
)";

} // namespace

TEST(YamlCppStruct, ParseEverything) {
    const auto [cfg, error] = parse<Everything>(kEverythingYaml);
    ASSERT_TRUE(cfg.has_value()) << error;
    EXPECT_TRUE(error.empty());

    EXPECT_EQ(cfg->ch, 'z');
    EXPECT_DOUBLE_EQ(cfg->price, 12.5);
    EXPECT_EQ(cfg->count, 42);
    EXPECT_EQ(cfg->text, std::string("hello"));
    EXPECT_EQ(cfg->vec, (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(cfg->u8set, (std::unordered_set<uint8_t>{9, 5, 7}));
    EXPECT_EQ(cfg->iset, (std::set<int>{1, 2, 3}));
    EXPECT_EQ(cfg->umap.size(), std::size_t{2});
    EXPECT_EQ(cfg->umap.at("one"), 1);
    EXPECT_EQ(cfg->umap.at("two"), 2);
    EXPECT_EQ(cfg->arr, (std::array<int, 2>{{10, 20}}));
    EXPECT_EQ(cfg->list, (std::list<int>{4, 5}));
    EXPECT_EQ(cfg->pair, (std::pair<int, std::string>{1, "one"}));
    EXPECT_EQ(std::get<0>(cfg->tup), std::string("tuple"));
    EXPECT_EQ(std::get<1>(cfg->tup), 89);
    ASSERT_TRUE(cfg->opt.has_value());
    EXPECT_EQ(*cfg->opt, std::string("value"));
    EXPECT_EQ(cfg->msec, std::chrono::milliseconds{1500});
    ASSERT_TRUE(std::holds_alternative<std::vector<int>>(cfg->var));
    EXPECT_EQ(std::get<std::vector<int>>(cfg->var), (std::vector<int>{8, 5, 65}));
    EXPECT_EQ(cfg->color, Color::Green);
    EXPECT_EQ(cfg->inner.value, 99);
    EXPECT_EQ(cfg->inner.label, std::string("nested"));
}

TEST(YamlCppStruct, MissingFieldsApplyDefaults) {
    const auto [empty, empty_error] = parse<DefaultsOnly>("");
    ASSERT_TRUE(empty.has_value()) << empty_error;
    EXPECT_EQ(empty->a, 0);
    EXPECT_EQ(empty->b, std::string("default"));
    EXPECT_FALSE(empty->c.has_value());

    const auto [partial, partial_error] = parse<DefaultsOnly>("a: 5\n");
    ASSERT_TRUE(partial.has_value()) << partial_error;
    EXPECT_EQ(partial->a, 5);
    EXPECT_EQ(partial->b, std::string("default"));
    EXPECT_FALSE(partial->c.has_value());

    const auto [value, value_error] = parse<DefaultsOnly>("c: 3\n");
    ASSERT_TRUE(value.has_value()) << value_error;
    EXPECT_EQ(value->a, 0);
    ASSERT_TRUE(value->c.has_value());
    EXPECT_EQ(*value->c, 3);
}

TEST(YamlCppStruct, MissingRequiredField) {
    const auto [mixed, mixed_error] = parse<MixedRequired>("optional_field: x\n");
    EXPECT_FALSE(mixed.has_value());
    EXPECT_NE(mixed_error.find("missing field `required_field`"), std::string::npos);
    EXPECT_NE(mixed_error.find("MixedRequired"), std::string::npos);

    const auto [empty, empty_error] = parse<TextOnly>("");
    EXPECT_FALSE(empty.has_value());
    EXPECT_NE(empty_error.find("missing field `text`"), std::string::npos);
}

TEST(YamlCppStruct, InvalidFieldReportsPath) {
    const auto [cfg, error] = parse<TextOnly>("text: [1, 2]\n");
    EXPECT_FALSE(cfg.has_value());
    EXPECT_NE(error.find("text"), std::string::npos);
    EXPECT_NE(error.find("TextOnly"), std::string::npos);
}

TEST(YamlCppStruct, NestedErrorReportsPath) {
    const auto [cfg, error] = parse<NestedOnly>("inner:\n  value: not-an-int\n  label: x\n");
    EXPECT_FALSE(cfg.has_value());
    EXPECT_NE(error.find("inner"), std::string::npos);
    EXPECT_NE(error.find("value"), std::string::npos);
    EXPECT_NE(error.find("Inner"), std::string::npos);
}

TEST(YamlCppStruct, NullFields) {
    const auto [optional_null, optional_error] = parse<OptionalFields>("value: 1\nopt: ~\n");
    ASSERT_TRUE(optional_null.has_value()) << optional_error;
    EXPECT_FALSE(optional_null->opt.has_value());

    const auto [missing, missing_error] = parse<OptionalFields>("value: 1\n");
    ASSERT_TRUE(missing.has_value()) << missing_error;
    EXPECT_FALSE(missing->opt.has_value());

    const auto [required_null, required_null_error] = parse<TextOnly>("text: ~\n");
    EXPECT_FALSE(required_null.has_value());
    EXPECT_NE(required_null_error.find("is null"), std::string::npos);
}

TEST(YamlCppStruct, Uint8Range) {
    const auto [ok, ok_error] = parse<ByteFields>("byte: 200\nsbyte: -100\n");
    ASSERT_TRUE(ok.has_value()) << ok_error;
    EXPECT_EQ(ok->byte, 200);
    EXPECT_EQ(ok->sbyte, -100);

    const auto [overflow, overflow_error] = parse<ByteFields>("byte: 300\nsbyte: 0\n");
    EXPECT_FALSE(overflow.has_value());
    EXPECT_NE(overflow_error.find("uint8_t"), std::string::npos);
    EXPECT_NE(overflow_error.find("range"), std::string::npos);

    const auto [underflow, underflow_error] = parse<ByteFields>("byte: 0\nsbyte: -129\n");
    EXPECT_FALSE(underflow.has_value());
    EXPECT_NE(underflow_error.find("int8_t"), std::string::npos);
    EXPECT_NE(underflow_error.find("range"), std::string::npos);
}

TEST(YamlCppStruct, Encode8BitAsNumbers) {
    const ByteFields bytes{200, -100};
    const auto [encoded, error] = yaml_cpp_struct::to_yaml(bytes);
    ASSERT_TRUE(encoded.has_value()) << error;
    EXPECT_NE(encoded->find("200"), std::string::npos);
    EXPECT_NE(encoded->find("-100"), std::string::npos);

    const auto [scalar, scalar_error] = yaml_cpp_struct::to_yaml(uint8_t{42});
    ASSERT_TRUE(scalar.has_value()) << scalar_error;
    EXPECT_NE(scalar->find("42"), std::string::npos);
}

TEST(YamlCppStruct, OptionalFields) {
    const auto [present, present_error] = parse<OptionalFields>("value: 1\nopt: 5\n");
    ASSERT_TRUE(present.has_value()) << present_error;
    EXPECT_EQ(present->value, 1);
    ASSERT_TRUE(present->opt.has_value());
    EXPECT_EQ(*present->opt, 5);

    const auto [invalid, invalid_error] = parse<OptionalFields>("value: 1\nopt: not-a-number\n");
    EXPECT_FALSE(invalid.has_value());
    EXPECT_NE(invalid_error.find("opt"), std::string::npos);
}

TEST(YamlCppStruct, Enum) {
    const auto [by_name, name_error] = parse<Color>("\"Blue\"\n");
    ASSERT_TRUE(by_name.has_value()) << name_error;
    EXPECT_EQ(*by_name, Color::Blue);

    const auto [by_value, value_error] = parse<Color>("2\n");
    ASSERT_TRUE(by_value.has_value()) << value_error;
    EXPECT_EQ(*by_value, Color::Green);

    const auto [invalid_name, invalid_name_error] = parse<Color>("\"Purple\"\n");
    EXPECT_FALSE(invalid_name.has_value());
    EXPECT_NE(invalid_name_error.find("Purple"), std::string::npos);

    const auto [invalid_value, invalid_value_error] = parse<Color>("99\n");
    EXPECT_FALSE(invalid_value.has_value());
    EXPECT_FALSE(invalid_value_error.empty());

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(static_cast<Color>(99));
    EXPECT_FALSE(encoded.has_value());
    EXPECT_FALSE(encode_error.empty());
}

TEST(YamlCppStruct, RoundTrip) {
    const auto [cfg, error] = parse<Everything>(kEverythingYaml);
    ASSERT_TRUE(cfg.has_value()) << error;

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(*cfg);
    ASSERT_TRUE(encoded.has_value()) << encode_error;

    const auto [round_trip, parse_error] = parse<Everything>(*encoded);
    ASSERT_TRUE(round_trip.has_value()) << parse_error;

    EXPECT_EQ(round_trip->ch, cfg->ch);
    EXPECT_DOUBLE_EQ(round_trip->price, cfg->price);
    EXPECT_EQ(round_trip->count, cfg->count);
    EXPECT_EQ(round_trip->text, cfg->text);
    EXPECT_EQ(round_trip->vec, cfg->vec);
    EXPECT_EQ(round_trip->u8set, cfg->u8set);
    EXPECT_EQ(round_trip->iset, cfg->iset);
    EXPECT_EQ(round_trip->umap, cfg->umap);
    EXPECT_EQ(round_trip->arr, cfg->arr);
    EXPECT_EQ(round_trip->list, cfg->list);
    EXPECT_EQ(round_trip->pair, cfg->pair);
    EXPECT_EQ(round_trip->tup, cfg->tup);
    EXPECT_EQ(round_trip->opt, cfg->opt);
    EXPECT_EQ(round_trip->msec, cfg->msec);
    EXPECT_EQ(round_trip->var, cfg->var);
    EXPECT_EQ(round_trip->color, cfg->color);
    EXPECT_EQ(round_trip->inner.value, cfg->inner.value);
    EXPECT_EQ(round_trip->inner.label, cfg->inner.label);
}

TEST(YamlCppStruct, FileErrors) {
    const auto [cfg, error] = yaml_cpp_struct::from_yaml<TextOnly>("/definitely/not/here.yaml");
    EXPECT_FALSE(cfg.has_value());
    EXPECT_NE(error.find("not found"), std::string::npos);
}

TEST(YamlCppStruct, EnvOverridesFromFile) {
    namespace fs = std::filesystem;
    const fs::path path = fs::temp_directory_path() / "yaml_cpp_struct_env_test.yaml";

    {
        std::ofstream out(path);
        ASSERT_TRUE(static_cast<bool>(out));
        out << "name: original\nnested:\n  value: 5\n  label: file\n";
    }

    const auto result = yaml_cpp_struct::from_yaml_result<EnvConfig>(
        path.string(), make_env({{"APP_NAME", "from-env"}, {"APP_NESTED__LABEL", "env-label"}}));
    ASSERT_TRUE(result.value.has_value()) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ(result.value->name, std::string("from-env"));
    EXPECT_EQ(result.value->nested.value, 5);
    EXPECT_EQ(result.value->nested.label, std::string("env-label"));

    std::error_code ignored;
    fs::remove(path, ignored);

    const auto missing = yaml_cpp_struct::from_yaml_result<EnvConfig>("/definitely/not/here.yaml", make_env({}));
    EXPECT_FALSE(missing.value.has_value());
    EXPECT_EQ(missing.error.code, yaml_cpp_struct::ErrorCode::BadFile);
}

TEST(YamlCppStruct, EnvOnlyAndPrecedence) {
    const auto only_env = yaml_cpp_struct::from_yaml_result<EnvOnly, false>("optional_field: file\n",
                                                                            make_env({{"APP_REQUIRED_FIELD", "42"}}));
    ASSERT_TRUE(only_env.value.has_value()) << yaml_cpp_struct::format_error(only_env.error);
    EXPECT_EQ(only_env.value->required_field, 42);
    EXPECT_EQ(only_env.value->optional_field, std::string("file"));

    const auto env_wins = yaml_cpp_struct::from_yaml_result<EnvOnly, false>(
        "required_field: 1\noptional_field: file\n",
        make_env({{"APP_REQUIRED_FIELD", "2"}, {"APP_OPTIONAL_FIELD", "env"}}));
    ASSERT_TRUE(env_wins.value.has_value()) << yaml_cpp_struct::format_error(env_wins.error);
    EXPECT_EQ(env_wins.value->required_field, 2);
    EXPECT_EQ(env_wins.value->optional_field, std::string("env"));

    const auto missing = yaml_cpp_struct::from_yaml_result<EnvOnly, false>("optional_field: file\n", make_env({}));
    EXPECT_FALSE(missing.value.has_value());
    EXPECT_EQ(missing.error.code, yaml_cpp_struct::ErrorCode::MissingField);
}

TEST(YamlCppStruct, EnvEmptyAndRawStrings) {
    const auto ignored = yaml_cpp_struct::from_yaml_result<EnvOnly, false>(
        "required_field: 1\noptional_field: file\n",
        make_env({{"APP_REQUIRED_FIELD", ""}, {"APP_OPTIONAL_FIELD", ""}}));
    ASSERT_TRUE(ignored.value.has_value()) << yaml_cpp_struct::format_error(ignored.error);
    EXPECT_EQ(ignored.value->required_field, 1);
    EXPECT_EQ(ignored.value->optional_field, std::string("file"));

    auto options = yaml_cpp_struct::env_options_for<EnvOnly>();
    options.ignore_empty = false;
    const auto empty_string = yaml_cpp_struct::from_yaml_result<EnvOnly, false>(
        "required_field: 1\noptional_field: file\n", options, make_env({{"APP_OPTIONAL_FIELD", ""}}));
    ASSERT_TRUE(empty_string.value.has_value()) << yaml_cpp_struct::format_error(empty_string.error);
    EXPECT_EQ(empty_string.value->optional_field, std::string(""));

    const auto raw = yaml_cpp_struct::from_yaml_result<EnvOnly, false>("required_field: 1\n",
                                                                       make_env({{"APP_OPTIONAL_FIELD", "001"}}));
    ASSERT_TRUE(raw.value.has_value()) << yaml_cpp_struct::format_error(raw.error);
    EXPECT_EQ(raw.value->optional_field, std::string("001"));
}

TEST(YamlCppStruct, EnvParseStringsAsYaml) {
    auto options = yaml_cpp_struct::env_options_for<EnvOnly>();
    options.parse_strings_as_yaml = true;
    const auto result = yaml_cpp_struct::from_yaml_result<EnvOnly, false>(
        "required_field: 1\n", options, make_env({{"APP_OPTIONAL_FIELD", "\"line1\\nline2\""}}));
    ASSERT_TRUE(result.value.has_value()) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ(result.value->optional_field, std::string("line1\nline2"));
}

TEST(YamlCppStruct, EnvListSeparator) {
    auto options = yaml_cpp_struct::env_options_for<EnvLists>();

    const auto yaml_list =
        yaml_cpp_struct::from_yaml_result<EnvLists, false>("", options, make_env({{"APP_VALUES", "[4, 5]"}}));
    ASSERT_TRUE(yaml_list.value.has_value()) << yaml_cpp_struct::format_error(yaml_list.error);
    EXPECT_EQ(yaml_list.value->values, (std::vector<int>{4, 5}));

    options.list_separator = ',';
    const auto comma_list =
        yaml_cpp_struct::from_yaml_result<EnvLists, false>("", options, make_env({{"APP_VALUES", "1,2,3"}}));
    ASSERT_TRUE(comma_list.value.has_value()) << yaml_cpp_struct::format_error(comma_list.error);
    EXPECT_EQ(comma_list.value->values, (std::vector<int>{1, 2, 3}));
}

TEST(YamlCppStruct, ExplicitEnvNames) {
    const auto defaulted = yaml_cpp_struct::from_env_result<ExplicitEnv>(make_env({{"CUSTOM_PORT", "8080"}}));
    ASSERT_TRUE(defaulted.value.has_value()) << yaml_cpp_struct::format_error(defaulted.error);
    EXPECT_EQ(defaulted.value->port, 8080);
    EXPECT_EQ(defaulted.value->timeout, 30);

    const auto explicit_timeout =
        yaml_cpp_struct::from_env_result<ExplicitEnv>(make_env({{"CUSTOM_PORT", "8080"}, {"CUSTOM_TIMEOUT", "60"}}));
    ASSERT_TRUE(explicit_timeout.value.has_value()) << yaml_cpp_struct::format_error(explicit_timeout.error);
    EXPECT_EQ(explicit_timeout.value->port, 8080);
    EXPECT_EQ(explicit_timeout.value->timeout, 60);
}

TEST(YamlCppStruct, ExplicitNestedEnvName) {
    const auto nested = yaml_cpp_struct::from_env_result<ExplicitNested>(
        make_env({{"CUSTOM_INNER__VALUE", "42"}, {"CUSTOM_INNER__LABEL", "nested"}}));
    ASSERT_TRUE(nested.value.has_value()) << yaml_cpp_struct::format_error(nested.error);
    EXPECT_EQ(nested.value->inner.value, 42);
    EXPECT_EQ(nested.value->inner.label, std::string("nested"));

    const auto whole =
        yaml_cpp_struct::from_env_result<ExplicitNested>(make_env({{"CUSTOM_INNER", "{value: 7, label: whole}"}}));
    ASSERT_TRUE(whole.value.has_value()) << yaml_cpp_struct::format_error(whole.error);
    EXPECT_EQ(whole.value->inner.value, 7);
    EXPECT_EQ(whole.value->inner.label, std::string("whole"));
}

TEST(YamlCppStruct, EnvErrors) {
    const auto duplicate = yaml_cpp_struct::from_env_result<DuplicateEnv>(make_env({{"SAME_ENV", "1"}}));
    EXPECT_FALSE(duplicate.value.has_value());
    EXPECT_EQ(duplicate.error.code, yaml_cpp_struct::ErrorCode::EnvDuplicate);

    const auto conflict =
        yaml_cpp_struct::from_yaml_result<EnvConfig, false>("nested: 5\n", make_env({{"APP_NESTED__LABEL", "x"}}));
    EXPECT_FALSE(conflict.value.has_value());
    EXPECT_EQ(conflict.error.code, yaml_cpp_struct::ErrorCode::EnvConflict);

    const auto parse = yaml_cpp_struct::from_env_result<EnvOnly>(make_env({{"APP_REQUIRED_FIELD", "not-an-int"}}));
    EXPECT_FALSE(parse.value.has_value());
    EXPECT_EQ(parse.error.code, yaml_cpp_struct::ErrorCode::EnvParseError);
    EXPECT_EQ(parse.error.env_name, std::string("APP_REQUIRED_FIELD"));
    EXPECT_EQ(parse.error.path, std::string("required_field"));
    EXPECT_EQ(parse.error.message.find("not-an-int"), std::string::npos);
}

TEST(YamlCppStruct, NoEnvPrefixMeansNoEnv) {
    const auto result = yaml_cpp_struct::from_yaml_result<NoPrefix, false>("", make_env({{"PORT", "9"}}));
    ASSERT_TRUE(result.value.has_value()) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ(result.value->port, 7);
}

TEST(YamlCppStruct, FromEnvWithoutFile) {
    const auto result = yaml_cpp_struct::from_env_result<EnvOnly>(make_env({{"APP_REQUIRED_FIELD", "7"}}));
    ASSERT_TRUE(result.value.has_value()) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ(result.value->required_field, 7);
    EXPECT_EQ(result.value->optional_field, std::string("default"));
}

TEST(YamlCppStruct, EnvAudit) {
    std::vector<std::pair<std::string, std::string>> applied;
    auto options = yaml_cpp_struct::env_options_for<EnvOnly>();
    options.on_applied = [&](std::string_view env, std::string_view path) {
        applied.emplace_back(std::string(env), std::string(path));
    };

    const auto result =
        yaml_cpp_struct::from_yaml_result<EnvOnly, false>("", options, make_env({{"APP_REQUIRED_FIELD", "1"}}));
    ASSERT_TRUE(result.value.has_value()) << yaml_cpp_struct::format_error(result.error);
    ASSERT_EQ(applied.size(), std::size_t{1});
    EXPECT_EQ(applied[0].first, std::string("APP_REQUIRED_FIELD"));
    EXPECT_EQ(applied[0].second, std::string("required_field"));
}

TEST(YamlCppStruct, YamlToJson) {
    const auto j = yaml_cpp_struct::yaml_to_json(R"(
a: 1
b: [1, 2]
c: {d: true}
e: null
empty_seq: []
empty_map: {}
num: 1.5
big: 10000000000
)");

    EXPECT_EQ(j["a"], 1);
    ASSERT_TRUE(j["b"].is_array());
    EXPECT_EQ(j["b"].size(), std::size_t{2});
    EXPECT_EQ(j["c"]["d"], true);
    EXPECT_TRUE(j["e"].is_null());
    ASSERT_TRUE(j["empty_seq"].is_array());
    EXPECT_TRUE(j["empty_seq"].empty());
    ASSERT_TRUE(j["empty_map"].is_object());
    EXPECT_TRUE(j["empty_map"].empty());
    EXPECT_EQ(j["num"], 1.5);
    EXPECT_TRUE(j["big"].is_number_integer());
    EXPECT_EQ(j["big"].get<long long>(), 10000000000LL);

    const auto list = yaml_cpp_struct::yaml_to_json("- 1\n- 2\n");
    ASSERT_TRUE(list.is_array());
    EXPECT_EQ(list.size(), std::size_t{2});

    const auto empty = yaml_cpp_struct::yaml_to_json("");
    EXPECT_TRUE(empty.is_null());
}

TEST(YamlCppStruct, PerFieldRequired) {
    const auto [missing, missing_error] = parse<MixedRequired>("optional_field: x\n");
    EXPECT_FALSE(missing.has_value());
    EXPECT_NE(missing_error.find("required_field"), std::string::npos);
    EXPECT_NE(missing_error.find("MixedRequired"), std::string::npos);

    const auto [ok, ok_error] = parse<MixedRequired>("required_field: 3\n");
    ASSERT_TRUE(ok.has_value()) << ok_error;
    EXPECT_EQ(ok->required_field, 3);
    EXPECT_EQ(ok->optional_field, std::string("default"));
}

TEST(YamlCppStruct, StructuredErrors) {
    const auto invalid = yaml_cpp_struct::from_yaml_result<TextOnly, false>("text: [1, 2]\n");
    EXPECT_FALSE(invalid.value.has_value());
    EXPECT_EQ(invalid.error.code, yaml_cpp_struct::ErrorCode::InvalidField);
    EXPECT_EQ(invalid.error.path, std::string("text"));
    EXPECT_GE(invalid.error.line, 0);
    EXPECT_GE(invalid.error.column, 0);

    const auto nested = yaml_cpp_struct::from_yaml_result<NestedOnly, false>("inner:\n  value: nope\n  label: x\n");
    EXPECT_FALSE(nested.value.has_value());
    EXPECT_EQ(nested.error.code, yaml_cpp_struct::ErrorCode::InvalidField);
    EXPECT_EQ(nested.error.path, std::string("inner.value"));

    const auto missing = yaml_cpp_struct::from_yaml_result<MixedRequired, false>("optional_field: x\n");
    EXPECT_EQ(missing.error.code, yaml_cpp_struct::ErrorCode::MissingField);
    EXPECT_EQ(missing.error.path, std::string("required_field"));

    const auto not_a_map = yaml_cpp_struct::from_yaml_result<TextOnly, false>("42\n");
    EXPECT_EQ(not_a_map.error.code, yaml_cpp_struct::ErrorCode::NotAMap);

    const auto empty_required = yaml_cpp_struct::from_yaml_result<TextOnly, false>("");
    EXPECT_EQ(empty_required.error.code, yaml_cpp_struct::ErrorCode::MissingField);

    const auto no_file = yaml_cpp_struct::from_yaml_result<TextOnly>("/definitely/not/here.yaml");
    EXPECT_EQ(no_file.error.code, yaml_cpp_struct::ErrorCode::BadFile);

    const auto parse_error = yaml_cpp_struct::from_yaml_result<TextOnly, false>("a: [1, 2\n");
    EXPECT_EQ(parse_error.error.code, yaml_cpp_struct::ErrorCode::ParseError);
}

TEST(YamlCppStruct, NodeAndStreamApis) {
    YAML::Node node = YAML::Load("text: via-node\n");
    const auto from_node_result = yaml_cpp_struct::from_node_result<TextOnly>(node);
    ASSERT_TRUE(from_node_result.value.has_value());
    EXPECT_EQ(from_node_result.value->text, std::string("via-node"));

    const auto to_node_result = yaml_cpp_struct::to_node_result(TextOnly{"x"});
    ASSERT_TRUE(to_node_result.value.has_value());
    EXPECT_TRUE(to_node_result.value->IsMap());
    const auto again = yaml_cpp_struct::from_node_result<TextOnly>(*to_node_result.value);
    ASSERT_TRUE(again.value.has_value());
    EXPECT_EQ(again.value->text, std::string("x"));

    std::istringstream in("text: via-stream\n");
    const auto from_stream_result = yaml_cpp_struct::from_stream_result<TextOnly>(in);
    ASSERT_TRUE(from_stream_result.value.has_value());
    EXPECT_EQ(from_stream_result.value->text, std::string("via-stream"));

    std::ostringstream out;
    const auto stream_error = yaml_cpp_struct::to_stream(TextOnly{"x"}, out);
    EXPECT_EQ(stream_error.code, yaml_cpp_struct::ErrorCode::None);
    EXPECT_FALSE(out.str().empty());
}

TEST(YamlCppStruct, MultiDocument) {
    const auto result =
        yaml_cpp_struct::from_yaml_all_result<MixedRequired>("---\nrequired_field: 1\n---\nrequired_field: 2\n", false);
    ASSERT_TRUE(result.value.has_value());
    ASSERT_EQ(result.value->size(), std::size_t{2});
    EXPECT_EQ((*result.value)[0].required_field, 1);
    EXPECT_EQ((*result.value)[1].required_field, 2);
}

TEST(YamlCppStruct, ExtendedContainers) {
    using DescendingSet = std::set<int, std::greater<int>>;
    const auto [desc, desc_error] = parse<DescendingSet>("[3, 1, 2]\n");
    ASSERT_TRUE(desc.has_value()) << desc_error;
    EXPECT_EQ(*desc, (DescendingSet{3, 2, 1}));

    using HashedSet = std::unordered_set<int, IntHash>;
    const auto [hashed, hashed_error] = parse<HashedSet>("[1, 2, 3]\n");
    ASSERT_TRUE(hashed.has_value()) << hashed_error;
    EXPECT_EQ(hashed->size(), std::size_t{3});

    using IntDeque = std::deque<int>;
    const auto [deque, deque_error] = parse<IntDeque>("[1, 2, 3]\n");
    ASSERT_TRUE(deque.has_value()) << deque_error;
    EXPECT_EQ(*deque, (IntDeque{1, 2, 3}));

    using IntForwardList = std::forward_list<int>;
    const auto [forward, forward_error] = parse<IntForwardList>("[1, 2, 3]\n");
    ASSERT_TRUE(forward.has_value()) << forward_error;
    EXPECT_EQ(std::vector<int>(forward->begin(), forward->end()), (std::vector<int>{1, 2, 3}));

    const auto [encoded_deque, encoded_deque_error] = yaml_cpp_struct::to_yaml(IntDeque{1, 2, 3});
    ASSERT_TRUE(encoded_deque.has_value()) << encoded_deque_error;
    EXPECT_NE(encoded_deque->find("- 2"), std::string::npos);

    const auto [encoded_forward, encoded_forward_error] = yaml_cpp_struct::to_yaml(IntForwardList{1, 2, 3});
    ASSERT_TRUE(encoded_forward.has_value()) << encoded_forward_error;
    EXPECT_NE(encoded_forward->find("- 2"), std::string::npos);

    const auto [path, path_error] = parse<std::filesystem::path>("\"/tmp/x y\"\n");
    ASSERT_TRUE(path.has_value()) << path_error;
    EXPECT_EQ(path->string(), std::string("/tmp/x y"));

    const auto [encoded_path, encoded_path_error] = yaml_cpp_struct::to_yaml(std::filesystem::path{"/tmp/x y"});
    ASSERT_TRUE(encoded_path.has_value()) << encoded_path_error;
    EXPECT_NE(encoded_path->find("/tmp/x y"), std::string::npos);

    const auto [encoded_desc, encoded_desc_error] = yaml_cpp_struct::to_yaml(DescendingSet{3, 2, 1});
    ASSERT_TRUE(encoded_desc.has_value()) << encoded_desc_error;
    const auto p1 = encoded_desc->find("- 3");
    const auto p2 = encoded_desc->find("- 2");
    const auto p3 = encoded_desc->find("- 1");
    EXPECT_NE(p1, std::string::npos);
    EXPECT_NE(p2, std::string::npos);
    EXPECT_NE(p3, std::string::npos);
    EXPECT_TRUE(p1 < p2 && p2 < p3);
}

TEST(YamlCppStruct, MonostateVariant) {
    using MaybeInt = std::variant<std::monostate, int>;

    const auto [empty, empty_error] = parse<MaybeInt>("~\n");
    ASSERT_TRUE(empty.has_value()) << empty_error;
    EXPECT_TRUE(std::holds_alternative<std::monostate>(*empty));

    const auto [value, value_error] = parse<MaybeInt>("42\n");
    ASSERT_TRUE(value.has_value()) << value_error;
    ASSERT_TRUE(std::holds_alternative<int>(*value));
    EXPECT_EQ(std::get<int>(*value), 42);
}

TEST(YamlCppStruct, ScalarTypes) {
    const char *yaml = R"(
boolean: true
f: 1.5
d: 2.5
ld: 3.5
i8: -8
u8: 8
i16: -16
u16: 16
i32: -32
u32: 32
i64: -64
u64: 64
s: -2
us: 2
l: -3
ul: 3
ll: -4
ull: 4
)";
    const auto [cfg, error] = parse<Scalars>(yaml);
    ASSERT_TRUE(cfg.has_value()) << error;
    EXPECT_TRUE(cfg->boolean);
    EXPECT_FLOAT_EQ(cfg->f, 1.5f);
    EXPECT_DOUBLE_EQ(cfg->d, 2.5);
    EXPECT_DOUBLE_EQ(static_cast<double>(cfg->ld), 3.5);
    EXPECT_EQ(cfg->i8, -8);
    EXPECT_EQ(cfg->u8, 8);
    EXPECT_EQ(cfg->i16, -16);
    EXPECT_EQ(cfg->u16, 16);
    EXPECT_EQ(cfg->i32, -32);
    EXPECT_EQ(cfg->u32, 32u);
    EXPECT_EQ(cfg->i64, -64);
    EXPECT_EQ(cfg->u64, 64u);
    EXPECT_EQ(cfg->s, -2);
    EXPECT_EQ(cfg->us, 2);
    EXPECT_EQ(cfg->l, -3);
    EXPECT_EQ(cfg->ul, 3u);
    EXPECT_EQ(cfg->ll, -4);
    EXPECT_EQ(cfg->ull, 4u);

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(*cfg);
    ASSERT_TRUE(encoded.has_value()) << encode_error;
    const auto [round_trip, parse_error] = parse<Scalars>(*encoded);
    ASSERT_TRUE(round_trip.has_value()) << parse_error;
    EXPECT_EQ(round_trip->boolean, cfg->boolean);
    EXPECT_FLOAT_EQ(round_trip->f, cfg->f);
    EXPECT_DOUBLE_EQ(round_trip->d, cfg->d);
    EXPECT_DOUBLE_EQ(static_cast<double>(round_trip->ld), static_cast<double>(cfg->ld));
    EXPECT_EQ(round_trip->i8, cfg->i8);
    EXPECT_EQ(round_trip->u8, cfg->u8);
    EXPECT_EQ(round_trip->i16, cfg->i16);
    EXPECT_EQ(round_trip->u16, cfg->u16);
    EXPECT_EQ(round_trip->i32, cfg->i32);
    EXPECT_EQ(round_trip->u32, cfg->u32);
    EXPECT_EQ(round_trip->i64, cfg->i64);
    EXPECT_EQ(round_trip->u64, cfg->u64);
    EXPECT_EQ(round_trip->s, cfg->s);
    EXPECT_EQ(round_trip->us, cfg->us);
    EXPECT_EQ(round_trip->l, cfg->l);
    EXPECT_EQ(round_trip->ul, cfg->ul);
    EXPECT_EQ(round_trip->ll, cfg->ll);
    EXPECT_EQ(round_trip->ull, cfg->ull);
}

TEST(YamlCppStruct, TopLevelScalars) {
    const auto [boolean, boolean_error] = parse<bool>("true\n");
    ASSERT_TRUE(boolean.has_value()) << boolean_error;
    EXPECT_TRUE(*boolean);

    const auto [integer, integer_error] = parse<int>("42\n");
    ASSERT_TRUE(integer.has_value()) << integer_error;
    EXPECT_EQ(*integer, 42);

    const auto [text, text_error] = parse<std::string>("hello\n");
    ASSERT_TRUE(text.has_value()) << text_error;
    EXPECT_EQ(*text, std::string("hello"));

    const auto [character, character_error] = parse<char>("z\n");
    ASSERT_TRUE(character.has_value()) << character_error;
    EXPECT_EQ(*character, 'z');

    const auto [single, single_error] = parse<float>("1.5\n");
    ASSERT_TRUE(single.has_value()) << single_error;
    EXPECT_FLOAT_EQ(*single, 1.5f);

    const auto [wide, wide_error] = parse<long double>("2.5\n");
    ASSERT_TRUE(wide.has_value()) << wide_error;
    EXPECT_DOUBLE_EQ(static_cast<double>(*wide), 2.5);

    const auto [encoded_text, encoded_text_error] = yaml_cpp_struct::to_yaml(std::string("hello"));
    ASSERT_TRUE(encoded_text.has_value()) << encoded_text_error;
    EXPECT_NE(encoded_text->find("hello"), std::string::npos);

    const auto [encoded_bool, encoded_bool_error] = yaml_cpp_struct::to_yaml(true);
    ASSERT_TRUE(encoded_bool.has_value()) << encoded_bool_error;
    EXPECT_NE(encoded_bool->find("true"), std::string::npos);

    const auto [encoded_int, encoded_int_error] = yaml_cpp_struct::to_yaml(42);
    ASSERT_TRUE(encoded_int.has_value()) << encoded_int_error;
    EXPECT_NE(encoded_int->find("42"), std::string::npos);
}

TEST(YamlCppStruct, OrderedAndCustomMaps) {
    const char *yaml = R"(
ordered: {b: 2, a: 1}
descending: {1: one, 2: two}
hashed: {1: 10, 2: 20}
)";
    const auto [cfg, error] = parse<Maps>(yaml);
    ASSERT_TRUE(cfg.has_value()) << error;
    EXPECT_EQ(cfg->ordered.at("a"), 1);
    EXPECT_EQ(cfg->ordered.at("b"), 2);
    EXPECT_EQ(cfg->descending.at(1), std::string("one"));
    EXPECT_EQ(cfg->descending.at(2), std::string("two"));
    EXPECT_EQ(cfg->hashed.at(1), 10);
    EXPECT_EQ(cfg->hashed.at(2), 20);

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(*cfg);
    ASSERT_TRUE(encoded.has_value()) << encode_error;
    const auto [round_trip, parse_error] = parse<Maps>(*encoded);
    ASSERT_TRUE(round_trip.has_value()) << parse_error;
    EXPECT_EQ(round_trip->ordered, cfg->ordered);
    EXPECT_EQ(round_trip->descending, cfg->descending);
    EXPECT_EQ(round_trip->hashed, cfg->hashed);
}

TEST(YamlCppStruct, ComplexDefaultExpressions) {
    const auto [cfg, error] = parse<ComplexDefaults>("");
    ASSERT_TRUE(cfg.has_value()) << error;
    ASSERT_EQ(cfg->map.size(), std::size_t{1});
    EXPECT_EQ(cfg->map.at("a"), 1);
    EXPECT_EQ(std::get<0>(cfg->tup), 1);
    EXPECT_EQ(std::get<1>(cfg->tup), std::string("x"));
    EXPECT_EQ(cfg->vec, (std::vector<int>{1, 2, 3}));
}

TEST(YamlCppStruct, OptionalContainers) {
    const auto [missing, missing_error] = parse<OptionalContainers>("");
    ASSERT_TRUE(missing.has_value()) << missing_error;
    EXPECT_FALSE(missing->opt_vec.has_value());
    EXPECT_FALSE(missing->opt_map.has_value());

    const auto [present, present_error] = parse<OptionalContainers>("opt_vec: [1, 2]\nopt_map: {a: 1}\n");
    ASSERT_TRUE(present.has_value()) << present_error;
    ASSERT_TRUE(present->opt_vec.has_value());
    EXPECT_EQ(*present->opt_vec, (std::vector<int>{1, 2}));
    ASSERT_TRUE(present->opt_map.has_value());
    EXPECT_EQ(present->opt_map->at("a"), 1);

    const auto [nulls, nulls_error] = parse<OptionalContainers>("opt_vec: ~\nopt_map: ~\n");
    ASSERT_TRUE(nulls.has_value()) << nulls_error;
    EXPECT_FALSE(nulls->opt_vec.has_value());
    EXPECT_FALSE(nulls->opt_map.has_value());

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(missing.value());
    ASSERT_TRUE(encoded.has_value()) << encode_error;
    const auto [round_trip, parse_error] = parse<OptionalContainers>(*encoded);
    ASSERT_TRUE(round_trip.has_value()) << parse_error;
    EXPECT_FALSE(round_trip->opt_vec.has_value());
    EXPECT_FALSE(round_trip->opt_map.has_value());
}

TEST(YamlCppStruct, VariantAlternatives) {
    using Var = std::variant<int, std::string>;

    const auto [integer, integer_error] = parse<Var>("42\n");
    ASSERT_TRUE(integer.has_value()) << integer_error;
    ASSERT_TRUE(std::holds_alternative<int>(*integer));
    EXPECT_EQ(std::get<int>(*integer), 42);

    const auto [text, text_error] = parse<Var>("\"hello\"\n");
    ASSERT_TRUE(text.has_value()) << text_error;
    ASSERT_TRUE(std::holds_alternative<std::string>(*text));
    EXPECT_EQ(std::get<std::string>(*text), std::string("hello"));

    const auto [no_match, no_match_error] = parse<Var>("[1, 2]\n");
    EXPECT_FALSE(no_match.has_value());
    EXPECT_NE(no_match_error.find("variant"), std::string::npos);

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(Var{std::string("x")});
    ASSERT_TRUE(encoded.has_value()) << encode_error;
    EXPECT_NE(encoded->find("x"), std::string::npos);
}

TEST(YamlCppStruct, TupleAndArraySizeValidation) {
    const auto [short_tuple, short_tuple_error] = parse<std::tuple<int, int>>("[1]\n");
    EXPECT_FALSE(short_tuple.has_value());
    EXPECT_FALSE(short_tuple_error.empty());

    const auto [short_array, short_array_error] = parse<std::array<int, 2>>("[1]\n");
    EXPECT_FALSE(short_array.has_value());
    EXPECT_FALSE(short_array_error.empty());

    const auto [array, array_error] = parse<std::array<int, 2>>("[1, 2]\n");
    ASSERT_TRUE(array.has_value()) << array_error;
    EXPECT_EQ(*array, (std::array<int, 2>{{1, 2}}));

    const auto [tuple, tuple_error] = parse<std::tuple<int, std::string>>("[1, x]\n");
    ASSERT_TRUE(tuple.has_value()) << tuple_error;
    EXPECT_EQ(std::get<0>(*tuple), 1);
    EXPECT_EQ(std::get<1>(*tuple), std::string("x"));
}

TEST(YamlCppStruct, Valarray) {
    const auto [values, error] = parse<std::valarray<int>>("[1, 2, 3]\n");
    ASSERT_TRUE(values.has_value()) << error;
    ASSERT_EQ(values->size(), std::size_t{3});
    EXPECT_EQ((*values)[0], 1);
    EXPECT_EQ((*values)[1], 2);
    EXPECT_EQ((*values)[2], 3);

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(*values);
    ASSERT_TRUE(encoded.has_value()) << encode_error;
    const auto [round_trip, parse_error] = parse<std::valarray<int>>(*encoded);
    ASSERT_TRUE(round_trip.has_value()) << parse_error;
    ASSERT_EQ(round_trip->size(), std::size_t{3});
    EXPECT_EQ((*round_trip)[1], 2);
}

TEST(YamlCppStruct, ErrorCodes) {
    const auto bad_enum = yaml_cpp_struct::from_yaml_result<Color, false>("\"Purple\"\n");
    EXPECT_EQ(bad_enum.error.code, yaml_cpp_struct::ErrorCode::BadEnum);

    const auto bad_enum_encode = yaml_cpp_struct::to_yaml_result(static_cast<Color>(99));
    EXPECT_EQ(bad_enum_encode.error.code, yaml_cpp_struct::ErrorCode::BadEnum);

    const auto out_of_range = yaml_cpp_struct::from_yaml_result<ByteFields, false>("byte: 300\nsbyte: 0\n");
    EXPECT_EQ(out_of_range.error.code, yaml_cpp_struct::ErrorCode::OutOfRange);

    std::ostringstream bad_stream;
    bad_stream.setstate(std::ios::badbit);
    const auto stream_error = yaml_cpp_struct::to_stream(TextOnly{"x"}, bad_stream);
    EXPECT_EQ(stream_error.code, yaml_cpp_struct::ErrorCode::EmitError);
}

TEST(YamlCppStruct, MonostateEncoding) {
    using MaybeInt = std::variant<std::monostate, int>;

    const auto [encoded, error] = yaml_cpp_struct::to_yaml(MaybeInt{std::monostate{}});
    ASSERT_TRUE(encoded.has_value()) << error;

    const auto [decoded, decode_error] = parse<MaybeInt>(*encoded);
    ASSERT_TRUE(decoded.has_value()) << decode_error;
    EXPECT_TRUE(std::holds_alternative<std::monostate>(*decoded));
}

TEST(YamlCppStruct, StreamParseError) {
    std::istringstream in("a: [1, 2\n");
    const auto result = yaml_cpp_struct::from_stream_result<TextOnly>(in);
    EXPECT_FALSE(result.value.has_value());
    EXPECT_EQ(result.error.code, yaml_cpp_struct::ErrorCode::ParseError);
}

TEST(YamlCppStruct, EnvMalformedValue) {
    const auto result = yaml_cpp_struct::from_env_result<EnvOnly>(make_env({{"APP_REQUIRED_FIELD", "["}}));
    EXPECT_FALSE(result.value.has_value());
    EXPECT_EQ(result.error.code, yaml_cpp_struct::ErrorCode::EnvParseError);
    EXPECT_EQ(result.error.env_name, std::string("APP_REQUIRED_FIELD"));
    EXPECT_EQ(result.error.path, std::string("required_field"));
}

TEST(YamlCppStruct, MultiDocumentError) {
    const auto result =
        yaml_cpp_struct::from_yaml_all_result<MixedRequired>("---\nrequired_field: 1\n---\noptional_field: x\n", false);
    EXPECT_FALSE(result.value.has_value());
    EXPECT_EQ(result.error.code, yaml_cpp_struct::ErrorCode::MissingField);
}

TEST(YamlCppStruct, NestedContainers) {
    const auto [cfg, error] = parse<NestedContainers>("groups: {a: [1, 2], b: [3]}\nmaybe_values: [1, ~, 3]\n");
    ASSERT_TRUE(cfg.has_value()) << error;
    ASSERT_EQ(cfg->groups.size(), std::size_t{2});
    EXPECT_EQ(cfg->groups.at("a"), (std::vector<int>{1, 2}));
    EXPECT_EQ(cfg->groups.at("b"), (std::vector<int>{3}));
    ASSERT_EQ(cfg->maybe_values.size(), std::size_t{3});
    ASSERT_TRUE(cfg->maybe_values[0].has_value());
    EXPECT_EQ(*cfg->maybe_values[0], 1);
    EXPECT_FALSE(cfg->maybe_values[1].has_value());
    ASSERT_TRUE(cfg->maybe_values[2].has_value());
    EXPECT_EQ(*cfg->maybe_values[2], 3);

    const auto [encoded, encode_error] = yaml_cpp_struct::to_yaml(*cfg);
    ASSERT_TRUE(encoded.has_value()) << encode_error;
    const auto [round_trip, parse_error] = parse<NestedContainers>(*encoded);
    ASSERT_TRUE(round_trip.has_value()) << parse_error;
    EXPECT_EQ(round_trip->groups, cfg->groups);
    EXPECT_EQ(round_trip->maybe_values, cfg->maybe_values);
}
