#include <gtest/gtest.h>

#include <yaml_cpp_struct.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <forward_list>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

using yaml_cpp_struct::ErrorCode;

namespace {

yaml_cpp_struct::EnvLookup make_env(std::map<std::string, std::string, std::less<>> values) {
    return [values = std::move(values)](std::string_view name) -> std::optional<std::string> {
        const auto it = values.find(std::string(name));
        if (it == values.end())
            return std::nullopt;
        return it->second;
    };
}

} // namespace

// YCS_ADD_STRUCT / YCS_ADD_ENUM / YCS_ENV_PREFIX must be at global namespace
// scope, so the types below live outside the anonymous namespace.
enum class EnumOptionX {
    OptionA,
    OptionB,
};
YCS_ADD_ENUM(EnumOptionX, OptionA, OptionB)

enum class EnumOptionY {
    Alpha,
    Beta,
};
YCS_ADD_ENUM(EnumOptionY, Alpha, Beta)

struct LbmConfig {
    std::string topic;
    int port;

    YCS_DEFINE(topic, port)
};

// Mirrors the shape requested for the in-class YCS_DEFINE syntax.
struct InConfig {
    std::string topic;
    std::string plugin;
    LbmConfig lbm_config;
    std::unordered_map<std::string, std::string> plugin_parameters;
    bool hot_failover;
    std::vector<EnumOptionX> optlist_x;
    std::variant<EnumOptionY, std::vector<EnumOptionY>> optlist_y;

    YCS_DEFINE(topic, plugin, lbm_config, plugin_parameters, YCS_OPT(hot_failover, false),
               YCS_OPT(optlist_x, {EnumOptionX::OptionA}), optlist_y)
};

class SecretConfig {
  public:
    const std::string &host() const { return host_; }
    int port() const { return port_; }
    bool debug() const { return debug_; }
    const std::string &token() const { return token_; }

    YCS_DEFINE(host_, YCS_OPT(port_, 443), YCS_ENV_OPT(token_, "YCS_DEFINE_TOKEN", ""), YCS_OPT(debug_, false))

  private:
    std::string host_;
    int port_{0};
    std::string token_;
    bool debug_{false};
};
YCS_ENV_PREFIX(SecretConfig, "DEF_")

struct ChildDef {
    std::string name;
    int value;

    YCS_DEFINE(name, YCS_OPT(value, 1))
};

struct ParentDef {
    ChildDef child;
    bool enabled;

    YCS_DEFINE(child, YCS_OPT(enabled, false))
};
YCS_ENV_PREFIX(ParentDef, "P_")

struct Item {
    std::string name;
    int count;

    YCS_DEFINE(name, YCS_OPT(count, 0))
};

struct Bag {
    std::vector<Item> items;
    std::map<std::string, Item> by_name;
    std::unordered_map<std::string, Item> by_id;
    std::array<Item, 2> two;
    std::optional<Item> maybe;
    std::variant<Item, std::vector<Item>> choice;

    YCS_DEFINE(items, by_name, by_id, two, maybe, choice)
};

TEST(YcsDefine, DecodesWithDefaultsAndEnums) {
    const std::string yaml = R"(
topic: orders
plugin: lbm
lbm_config: { topic: inner, port: 9001 }
plugin_parameters: { a: b, c: d }
optlist_y: [Alpha, Beta]
)";

    auto result = yaml_cpp_struct::from_yaml_str<InConfig>(yaml);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);

    const InConfig &cfg = *result;
    EXPECT_EQ(cfg.topic, "orders");
    EXPECT_EQ(cfg.plugin, "lbm");
    EXPECT_EQ(cfg.lbm_config.topic, "inner");
    EXPECT_EQ(cfg.lbm_config.port, 9001);
    ASSERT_EQ(cfg.plugin_parameters.size(), 2u);
    EXPECT_EQ(cfg.plugin_parameters.at("a"), "b");
    EXPECT_FALSE(cfg.hot_failover);
    ASSERT_EQ(cfg.optlist_x.size(), 1u);
    EXPECT_EQ(cfg.optlist_x.front(), EnumOptionX::OptionA);

    ASSERT_TRUE(std::holds_alternative<std::vector<EnumOptionY>>(cfg.optlist_y));
    const auto &options = std::get<std::vector<EnumOptionY>>(cfg.optlist_y);
    ASSERT_EQ(options.size(), 2u);
    EXPECT_EQ(options[0], EnumOptionY::Alpha);
    EXPECT_EQ(options[1], EnumOptionY::Beta);
}

TEST(YcsDefine, DecodesScalarVariantAlternative) {
    const std::string yaml = R"(
topic: orders
plugin: lbm
lbm_config: { topic: inner, port: 9001 }
plugin_parameters: {}
optlist_y: Beta
)";

    auto result = yaml_cpp_struct::from_yaml_str<InConfig>(yaml);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    ASSERT_TRUE(std::holds_alternative<EnumOptionY>((*result).optlist_y));
    EXPECT_EQ(std::get<EnumOptionY>((*result).optlist_y), EnumOptionY::Beta);
}

TEST(YcsDefine, RoundTrips) {
    const std::string yaml = R"(
topic: orders
plugin: lbm
lbm_config: { topic: inner, port: 9001 }
plugin_parameters: { a: b }
hot_failover: true
optlist_x: [OptionB]
optlist_y: [Alpha]
)";

    auto parsed = yaml_cpp_struct::from_yaml_str<InConfig>(yaml);
    ASSERT_TRUE(parsed) << yaml_cpp_struct::format_error(parsed.error);

    auto emitted = yaml_cpp_struct::to_yaml(*parsed);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);

    auto reparsed = yaml_cpp_struct::from_yaml_str<InConfig>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);

    EXPECT_EQ((*reparsed).topic, "orders");
    EXPECT_EQ((*reparsed).lbm_config.port, 9001);
    EXPECT_TRUE((*reparsed).hot_failover);
    ASSERT_EQ((*reparsed).optlist_x.size(), 1u);
    EXPECT_EQ((*reparsed).optlist_x.front(), EnumOptionX::OptionB);
}

TEST(YcsDefine, PrivateFieldsAndEnvironmentOverride) {
    // Private fields are reachable because YCS_DEFINE befriends the visitors.
    auto base = yaml_cpp_struct::from_yaml_str<SecretConfig>("host_: example.com\n");
    ASSERT_TRUE(base) << yaml_cpp_struct::format_error(base.error);
    EXPECT_EQ((*base).host(), "example.com");
    EXPECT_EQ((*base).port(), 443);
    EXPECT_FALSE((*base).debug());

    auto options = yaml_cpp_struct::env_options_for<SecretConfig>();
    options.lookup = make_env({{"DEF_PORT_", "8443"},
                               {"DEF_DEBUG_", "true"},
                               // Explicit YCS_ENV names are absolute, not prefixed.
                               {"YCS_DEFINE_TOKEN", "s3cret"}});

    auto overridden = yaml_cpp_struct::from_yaml_str<SecretConfig>("host_: example.com\n", options);
    ASSERT_TRUE(overridden) << yaml_cpp_struct::format_error(overridden.error);
    EXPECT_EQ((*overridden).port(), 8443);
    EXPECT_TRUE((*overridden).debug());
    EXPECT_EQ((*overridden).token(), "s3cret");
}

TEST(YcsDefine, NestedStructEnvironmentOverride) {
    auto options = yaml_cpp_struct::env_options_for<ParentDef>();
    options.lookup = make_env({{"P_CHILD__VALUE", "42"}, {"P_ENABLED", "true"}});

    auto result = yaml_cpp_struct::from_yaml_str<ParentDef>("child: { name: file }\n", options);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ((*result).child.name, "file");
    EXPECT_EQ((*result).child.value, 42);
    EXPECT_TRUE((*result).enabled);
}

TEST(YcsDefine, ContainersOfDefinedStructs) {
    const std::string yaml = R"(
items:
  - { name: a, count: 1 }
  - { name: b }
by_name:
  first: { name: x, count: 3 }
by_id:
  id1: { name: y }
two:
  - { name: c }
  - { name: d, count: 2 }
maybe: { name: e, count: 4 }
choice:
  - { name: f }
  - { name: g }
)";

    auto result = yaml_cpp_struct::from_yaml_str<Bag>(yaml);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);

    const Bag &bag = *result;
    ASSERT_EQ(bag.items.size(), 2u);
    EXPECT_EQ(bag.items[0].name, "a");
    EXPECT_EQ(bag.items[0].count, 1);
    EXPECT_EQ(bag.items[1].count, 0);
    ASSERT_EQ(bag.by_name.size(), 1u);
    EXPECT_EQ(bag.by_name.at("first").count, 3);
    ASSERT_EQ(bag.by_id.size(), 1u);
    EXPECT_EQ(bag.by_id.at("id1").name, "y");
    EXPECT_EQ(bag.two[1].name, "d");
    ASSERT_TRUE(bag.maybe.has_value());
    EXPECT_EQ(bag.maybe->count, 4);
    ASSERT_TRUE(std::holds_alternative<std::vector<Item>>(bag.choice));
    EXPECT_EQ(std::get<std::vector<Item>>(bag.choice).size(), 2u);

    auto emitted = yaml_cpp_struct::to_yaml(bag);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<Bag>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    EXPECT_EQ((*reparsed).items.size(), 2u);
    EXPECT_EQ((*reparsed).by_name.at("first").name, "x");
    EXPECT_EQ((*reparsed).two[0].name, "c");
    EXPECT_EQ((*reparsed).maybe->name, "e");
}

TEST(YcsDefine, MissingRequiredField) {
    auto result = yaml_cpp_struct::from_yaml_str<Item>("count: 1");
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error.code, ErrorCode::MissingField);
}

// YCS_ADD_STRUCT and YCS_DEFINE types can be nested in each other.
struct Holder {
    Item item;
    std::vector<Item> items;
};
YCS_ADD_STRUCT(Holder, item, items)

TEST(YcsDefine, MixedWithAddStruct) {
    auto result = yaml_cpp_struct::from_yaml_str<Holder>("item: { name: one }\nitems: [{ name: two }]\n");
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ((*result).item.name, "one");
    ASSERT_EQ((*result).items.size(), 1u);
    EXPECT_EQ((*result).items.front().name, "two");
}

namespace defined_ns {

struct NamespacedChild {
    int id;

    YCS_DEFINE(id)
};

struct NamespacedParent {
    NamespacedChild child;

    YCS_DEFINE(child)
};

} // namespace defined_ns

struct EnvOnlyDef {
    std::string host;
    int port;

    YCS_DEFINE(host, YCS_OPT(port, 1))
};
YCS_ENV_PREFIX(EnvOnlyDef, "EOD_")

struct OptionalChildParent {
    std::optional<ChildDef> maybe;

    YCS_DEFINE(YCS_OPT(maybe, std::nullopt))
};
YCS_ENV_PREFIX(OptionalChildParent, "OCP_")

struct ContainerKinds {
    std::list<Item> list_items;
    std::deque<Item> deque_items;

    YCS_DEFINE(list_items, deque_items)
};

struct NullableDef {
    std::optional<int> opt;
    int req;

    YCS_DEFINE(req, YCS_OPT(opt, std::nullopt))
};

struct DuplicateDef {
    int a;
    int b;

    YCS_DEFINE(YCS_ENV(a, "DUP_ENV"), YCS_ENV(b, "DUP_ENV"))
};

TEST(YcsDefine, WorksInsideNamespace) {
    auto result = yaml_cpp_struct::from_yaml_str<defined_ns::NamespacedParent>("child: { id: 7 }");
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ((*result).child.id, 7);

    auto emitted = yaml_cpp_struct::to_yaml(*result);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    EXPECT_NE(emitted->find("id: 7"), std::string::npos);
}

TEST(YcsDefine, FromEnvironmentOnly) {
    auto options = yaml_cpp_struct::env_options_for<EnvOnlyDef>();
    options.lookup = make_env({{"EOD_HOST", "env.host"}, {"EOD_PORT", "9"}});

    auto result = yaml_cpp_struct::from_env<EnvOnlyDef>(options);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ((*result).host, "env.host");
    EXPECT_EQ((*result).port, 9);

    auto missing_options = yaml_cpp_struct::env_options_for<EnvOnlyDef>();
    missing_options.lookup = make_env({{"EOD_PORT", "9"}});
    auto missing = yaml_cpp_struct::from_env<EnvOnlyDef>(missing_options);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error.code, ErrorCode::MissingField);
}

TEST(YcsDefine, EnvironmentCanProvideRequiredField) {
    auto options = yaml_cpp_struct::env_options_for<SecretConfig>();
    options.lookup = make_env({{"DEF_HOST_", "env.example.com"}});

    auto result = yaml_cpp_struct::from_yaml_str<SecretConfig>("{}", options);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ((*result).host(), "env.example.com");
    EXPECT_EQ((*result).port(), 443);
    EXPECT_TRUE((*result).token().empty());
}

TEST(YcsDefine, WholeNestedValueWinsOverChildVariables) {
    auto options = yaml_cpp_struct::env_options_for<ParentDef>();
    options.lookup =
        make_env({{"P_CHILD", "{ name: env, value: 9 }"}, {"P_CHILD__VALUE", "123"}, {"P_ENABLED", "true"}});

    auto result = yaml_cpp_struct::from_yaml_str<ParentDef>("child: { name: file }\n", options);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ((*result).child.name, "env");
    EXPECT_EQ((*result).child.value, 9);
    EXPECT_TRUE((*result).enabled);
}

TEST(YcsDefine, OptionalNestedEnvironment) {
    auto child_options = yaml_cpp_struct::env_options_for<OptionalChildParent>();
    child_options.lookup = make_env({{"OCP_MAYBE__NAME", "n"}, {"OCP_MAYBE__VALUE", "5"}});

    auto child_result = yaml_cpp_struct::from_yaml_str<OptionalChildParent>("{}", child_options);
    ASSERT_TRUE(child_result) << yaml_cpp_struct::format_error(child_result.error);
    ASSERT_TRUE(child_result->maybe.has_value());
    EXPECT_EQ(child_result->maybe->name, "n");
    EXPECT_EQ(child_result->maybe->value, 5);

    auto whole_options = yaml_cpp_struct::env_options_for<OptionalChildParent>();
    whole_options.lookup = make_env({{"OCP_MAYBE", "{ name: whole, value: 6 }"}});

    auto whole_result = yaml_cpp_struct::from_yaml_str<OptionalChildParent>("{}", whole_options);
    ASSERT_TRUE(whole_result) << yaml_cpp_struct::format_error(whole_result.error);
    ASSERT_TRUE(whole_result->maybe.has_value());
    EXPECT_EQ(whole_result->maybe->name, "whole");
    EXPECT_EQ(whole_result->maybe->value, 6);
}

TEST(YcsDefine, EncodesBothVariantAlternatives) {
    Bag bag{};
    bag.two = {Item{"left", 1}, Item{"right", 2}};
    bag.maybe = Item{"maybe", 3};
    bag.choice = Item{"solo", 4};

    auto emitted = yaml_cpp_struct::to_yaml(bag);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);

    auto reparsed = yaml_cpp_struct::from_yaml_str<Bag>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    ASSERT_TRUE(std::holds_alternative<Item>((*reparsed).choice));
    EXPECT_EQ(std::get<Item>((*reparsed).choice).name, "solo");
    EXPECT_EQ(std::get<Item>((*reparsed).choice).count, 4);
}

TEST(YcsDefine, OptionalAndNullSemantics) {
    auto missing = yaml_cpp_struct::from_yaml_str<NullableDef>("req: 1");
    ASSERT_TRUE(missing) << yaml_cpp_struct::format_error(missing.error);
    EXPECT_FALSE(missing->opt.has_value());

    auto explicit_null = yaml_cpp_struct::from_yaml_str<NullableDef>("req: 1\nopt: null");
    ASSERT_TRUE(explicit_null) << yaml_cpp_struct::format_error(explicit_null.error);
    EXPECT_FALSE(explicit_null->opt.has_value());

    auto present = yaml_cpp_struct::from_yaml_str<NullableDef>("req: 1\nopt: 2");
    ASSERT_TRUE(present) << yaml_cpp_struct::format_error(present.error);
    ASSERT_TRUE(present->opt.has_value());
    EXPECT_EQ(*present->opt, 2);

    auto null_required = yaml_cpp_struct::from_yaml_str<Item>("name: null");
    ASSERT_FALSE(null_required);
    EXPECT_EQ(null_required.error.code, ErrorCode::InvalidField);
}

TEST(YcsDefine, ListAndDequeContainers) {
    const std::string yaml = R"(
list_items:
  - { name: l1, count: 1 }
  - { name: l2 }
deque_items:
  - { name: d1 }
)";

    auto result = yaml_cpp_struct::from_yaml_str<ContainerKinds>(yaml);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    ASSERT_EQ(result->list_items.size(), 2u);
    EXPECT_EQ(result->list_items.front().count, 1);
    EXPECT_EQ(result->deque_items.front().name, "d1");

    auto emitted = yaml_cpp_struct::to_yaml(*result);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<ContainerKinds>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    ASSERT_EQ(reparsed->list_items.size(), 2u);
    EXPECT_EQ(reparsed->list_items.back().name, "l2");
    EXPECT_EQ(reparsed->deque_items.front().name, "d1");
}

TEST(YcsDefine, NodeStreamAndMultiDocumentApis) {
    auto node = YAML::Load("child: { name: node, value: 2 }");
    auto from_node_result = yaml_cpp_struct::from_node<ParentDef>(node);
    ASSERT_TRUE(from_node_result) << yaml_cpp_struct::format_error(from_node_result.error);
    EXPECT_EQ(from_node_result->child.name, "node");

    std::istringstream stream("child: { name: stream, value: 3 }");
    auto stream_result = yaml_cpp_struct::from_yaml_stream<ParentDef>(stream);
    ASSERT_TRUE(stream_result) << yaml_cpp_struct::format_error(stream_result.error);
    EXPECT_EQ(stream_result->child.name, "stream");

    auto documents = yaml_cpp_struct::from_yaml_all_str<ParentDef>("child: { name: a }\n---\nchild: { name: b }\n");
    ASSERT_TRUE(documents) << yaml_cpp_struct::format_error(documents.error);
    ASSERT_EQ(documents->size(), 2u);
    EXPECT_EQ((*documents)[0].child.name, "a");
    EXPECT_EQ((*documents)[1].child.name, "b");
}

TEST(YcsDefine, EnvironmentErrorPaths) {
    auto parse_options = yaml_cpp_struct::env_options_for<SecretConfig>();
    parse_options.lookup = make_env({{"DEF_HOST_", "h"}, {"DEF_PORT_", "not-a-number"}});

    auto parse_error = yaml_cpp_struct::from_yaml_str<SecretConfig>("{}", parse_options);
    ASSERT_FALSE(parse_error);
    EXPECT_EQ(parse_error.error.code, ErrorCode::EnvParseError);
    EXPECT_NE(parse_error.error.message.find("DEF_PORT_"), std::string::npos);

    yaml_cpp_struct::EnvOptions duplicate_options;
    duplicate_options.enabled = true;
    duplicate_options.lookup = make_env({{"DUP_ENV", "1"}});

    auto duplicate = yaml_cpp_struct::from_yaml_str<DuplicateDef>("{}", duplicate_options);
    ASSERT_FALSE(duplicate);
    EXPECT_EQ(duplicate.error.code, ErrorCode::EnvDuplicate);
}

struct ComplexNesting {
    std::vector<std::vector<Item>> matrix;
    std::map<std::string, std::vector<Item>> groups;
    std::optional<std::vector<Item>> maybe_list;
    std::tuple<Item, int> pair_item;
    std::variant<Item, std::vector<Item>, std::map<std::string, Item>> multi;
    std::forward_list<Item> fwd;
    std::uint8_t byte;
    std::int8_t sbyte;
    std::chrono::milliseconds msec;
    std::filesystem::path path;

    YCS_DEFINE(matrix, groups, maybe_list, pair_item, multi, fwd, byte, sbyte, msec, path)
};

TEST(YcsDefine, DeeplyNestedStructures) {
    const std::string yaml = R"(
matrix:
  - [{ name: a, count: 1 }]
  - [{ name: b }]
groups:
  g1: [{ name: c }]
maybe_list: [{ name: d }]
pair_item: [{ name: e, count: 2 }, 7]
multi: { k: { name: f } }
fwd: [{ name: g }]
byte: 254
sbyte: -5
msec: 150
path: /tmp/x
)";

    auto result = yaml_cpp_struct::from_yaml_str<ComplexNesting>(yaml);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);

    const ComplexNesting &cfg = *result;
    ASSERT_EQ(cfg.matrix.size(), 2u);
    ASSERT_EQ(cfg.matrix[0].size(), 1u);
    EXPECT_EQ(cfg.matrix[0][0].name, "a");
    ASSERT_EQ(cfg.groups.at("g1").size(), 1u);
    EXPECT_EQ(cfg.groups.at("g1")[0].name, "c");
    ASSERT_TRUE(cfg.maybe_list.has_value());
    EXPECT_EQ(cfg.maybe_list->front().name, "d");
    EXPECT_EQ(std::get<0>(cfg.pair_item).name, "e");
    EXPECT_EQ(std::get<1>(cfg.pair_item), 7);
    using StringItemMap = std::map<std::string, Item>;
    ASSERT_TRUE(std::holds_alternative<StringItemMap>(cfg.multi));
    EXPECT_EQ(std::get<StringItemMap>(cfg.multi).at("k").name, "f");
    EXPECT_EQ(cfg.fwd.front().name, "g");
    EXPECT_EQ(cfg.byte, 254);
    EXPECT_EQ(cfg.sbyte, -5);
    EXPECT_EQ(cfg.msec.count(), 150);
    EXPECT_EQ(cfg.path, std::filesystem::path("/tmp/x"));

    auto emitted = yaml_cpp_struct::to_yaml(cfg);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<ComplexNesting>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    EXPECT_EQ(reparsed->matrix[1][0].name, "b");
    EXPECT_EQ(reparsed->groups.at("g1")[0].name, "c");
    EXPECT_EQ(reparsed->maybe_list->front().name, "d");
    EXPECT_EQ(reparsed->byte, 254);
    EXPECT_EQ(reparsed->path, std::filesystem::path("/tmp/x"));

    auto node = yaml_cpp_struct::to_node(cfg);
    ASSERT_TRUE(node) << yaml_cpp_struct::format_error(node.error);
    EXPECT_TRUE((*node)["matrix"].IsSequence());
}

struct Level3 {
    std::string id;
    std::optional<int> opt;

    YCS_DEFINE(id, YCS_OPT(opt, std::nullopt))
};

struct Level2 {
    Level3 level3;
    std::vector<Level3> items;

    YCS_DEFINE(level3, items)
};

struct Level1 {
    Level2 level2;
    bool enabled;

    YCS_DEFINE(level2, YCS_OPT(enabled, false))
};
YCS_ENV_PREFIX(Level1, "L1_")

TEST(YcsDefine, MultiLevelStructNesting) {
    const std::string yaml = R"(
level2:
  level3: { id: deep }
  items: [{ id: a }, { id: b }]
enabled: true
)";

    auto base = yaml_cpp_struct::from_yaml_str<Level1>(yaml);
    ASSERT_TRUE(base) << yaml_cpp_struct::format_error(base.error);
    EXPECT_EQ(base->level2.level3.id, "deep");
    EXPECT_FALSE(base->level2.level3.opt.has_value());
    ASSERT_EQ(base->level2.items.size(), 2u);
    EXPECT_EQ(base->level2.items[1].id, "b");
    EXPECT_TRUE(base->enabled);

    // Three-level-deep child variables: L1_ + LEVEL2__LEVEL3__ID.
    auto child_options = yaml_cpp_struct::env_options_for<Level1>();
    child_options.lookup =
        make_env({{"L1_LEVEL2__LEVEL3__ID", "env-deep"}, {"L1_LEVEL2__LEVEL3__OPT", "5"}, {"L1_ENABLED", "false"}});
    auto with_children = yaml_cpp_struct::from_yaml_str<Level1>(yaml, child_options);
    ASSERT_TRUE(with_children) << yaml_cpp_struct::format_error(with_children.error);
    EXPECT_EQ(with_children->level2.level3.id, "env-deep");
    ASSERT_TRUE(with_children->level2.level3.opt.has_value());
    EXPECT_EQ(*with_children->level2.level3.opt, 5);
    EXPECT_FALSE(with_children->enabled);
    EXPECT_EQ(with_children->level2.items.size(), 2u);

    // An intermediate whole-field value replaces everything below it.
    auto whole_options = yaml_cpp_struct::env_options_for<Level1>();
    whole_options.lookup = make_env({{"L1_LEVEL2", "{ level3: { id: whole, opt: 9 }, items: [{ id: replaced }] }"},
                                     {"L1_LEVEL2__LEVEL3__ID", "ignored"}});
    auto with_whole = yaml_cpp_struct::from_yaml_str<Level1>(yaml, whole_options);
    ASSERT_TRUE(with_whole) << yaml_cpp_struct::format_error(with_whole.error);
    EXPECT_EQ(with_whole->level2.level3.id, "whole");
    ASSERT_TRUE(with_whole->level2.level3.opt.has_value());
    EXPECT_EQ(*with_whole->level2.level3.opt, 9);
    ASSERT_EQ(with_whole->level2.items.size(), 1u);
    EXPECT_EQ(with_whole->level2.items[0].id, "replaced");

    auto emitted = yaml_cpp_struct::to_yaml(*with_whole);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<Level1>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    EXPECT_EQ(reparsed->level2.level3.id, "whole");
}

struct OrderedItem {
    int id;

    bool operator<(const OrderedItem &other) const { return id < other.id; }

    YCS_DEFINE(id)
};

struct SetHolder {
    std::set<OrderedItem> items;

    YCS_DEFINE(items)
};

TEST(YcsDefine, SetOfDefinedStructs) {
    auto result = yaml_cpp_struct::from_yaml_str<SetHolder>("items: [{ id: 3 }, { id: 1 }, { id: 2 }]");
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    ASSERT_EQ(result->items.size(), 3u);
    EXPECT_EQ(result->items.begin()->id, 1);
    EXPECT_EQ(result->items.rbegin()->id, 3);

    auto emitted = yaml_cpp_struct::to_yaml(*result);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<SetHolder>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    ASSERT_EQ(reparsed->items.size(), 3u);
    EXPECT_EQ(reparsed->items.begin()->id, 1);
}

class FullyPrivate {
  public:
    const std::string &id() const { return id_; }
    int count() const { return count_; }

  private:
    std::string id_;
    int count_{0};

    YCS_DEFINE(id_, YCS_OPT(count_, 2))
};
YCS_ENV_PREFIX(FullyPrivate, "FP_")

TEST(YcsDefine, PrivateSectionDefinition) {
    auto result = yaml_cpp_struct::from_yaml_str<FullyPrivate>("id_: private-id\n");
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ(result->id(), "private-id");
    EXPECT_EQ(result->count(), 2);

    auto options = yaml_cpp_struct::env_options_for<FullyPrivate>();
    options.lookup = make_env({{"FP_COUNT_", "7"}});
    auto overridden = yaml_cpp_struct::from_yaml_str<FullyPrivate>("id_: private-id\n", options);
    ASSERT_TRUE(overridden) << yaml_cpp_struct::format_error(overridden.error);
    EXPECT_EQ(overridden->count(), 7);

    auto emitted = yaml_cpp_struct::to_yaml(*result);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<FullyPrivate>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    EXPECT_EQ(reparsed->id(), "private-id");
}

struct TupleEnvConfig {
    std::tuple<std::string, int> pair;
    std::tuple<Item, int> with_struct;

    YCS_DEFINE(pair, with_struct)
};
YCS_ENV_PREFIX(TupleEnvConfig, "TUP_")

TEST(YcsDefine, EnvironmentTupleValues) {
    auto options = yaml_cpp_struct::env_options_for<TupleEnvConfig>();
    options.lookup = make_env({{"TUP_PAIR", "[\"text\", 89]"}, {"TUP_WITH_STRUCT", "[{ name: nested, count: 3 }, 7]"}});

    auto result = yaml_cpp_struct::from_yaml_str<TupleEnvConfig>("{}", options);
    ASSERT_TRUE(result) << yaml_cpp_struct::format_error(result.error);
    EXPECT_EQ(std::get<0>(result->pair), "text");
    EXPECT_EQ(std::get<1>(result->pair), 89);
    EXPECT_EQ(std::get<0>(result->with_struct).name, "nested");
    EXPECT_EQ(std::get<0>(result->with_struct).count, 3);
    EXPECT_EQ(std::get<1>(result->with_struct), 7);

    auto emitted = yaml_cpp_struct::to_yaml(*result);
    ASSERT_TRUE(emitted) << yaml_cpp_struct::format_error(emitted.error);
    auto reparsed = yaml_cpp_struct::from_yaml_str<TupleEnvConfig>(*emitted);
    ASSERT_TRUE(reparsed) << yaml_cpp_struct::format_error(reparsed.error);
    EXPECT_EQ(std::get<1>(reparsed->pair), 89);

    auto wrong_arity = yaml_cpp_struct::env_options_for<TupleEnvConfig>();
    wrong_arity.lookup = make_env({{"TUP_PAIR", "[\"only-one\"]"}, {"TUP_WITH_STRUCT", "[{ name: nested }, 7]"}});
    auto bad = yaml_cpp_struct::from_yaml_str<TupleEnvConfig>("{}", wrong_arity);
    ASSERT_FALSE(bad);
    EXPECT_EQ(bad.error.code, ErrorCode::EnvParseError);
    EXPECT_NE(bad.error.message.find("TUP_PAIR"), std::string::npos);
}
