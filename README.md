# yaml_cpp_struct

[![](https://github.com/fantasy-peak/yaml_cpp_struct/workflows/test/badge.svg)](https://github.com/fantasy-peak/yaml_cpp_struct/actions)

## function

It's a header-only wrapper. It's easy to map YAML documents to C++ structs.

- (De)serialization for structs, enums, `std::optional`, `std::variant`, `std::tuple`,
  `std::chrono::duration`, `std::vector`, `std::list`, `std::deque`,
  `std::forward_list`, `std::array`, `std::map`, `std::unordered_map`,
  `std::set`, `std::unordered_set` (including custom comparators/hashers),
  `std::filesystem::path`, ...
- `std::string_view` fields are rejected at compile time because the view would
  dangle; use `std::string` instead.
- Fields are required by default. `YCS_OPT(field, default)` declares an explicit
  default for a field that may be missing.
- Structured errors with field path, line and column, plus a `Result<T>` API.
- Node / stream / multi-document APIs (`from_node_result`, `from_stream_result`,
  `to_node_result`, `to_stream`, `from_yaml_all_result`).
- Automatic environment variable loading with per-struct prefixes, nested
  fields, explicit bindings and YAML value parsing.
- Optional YAML to JSON conversion with `OPEN_YAML_TO_JSON`.

## note

main branch need >= c++17, if you need to use c++11, please use cpp11 branch. cpp11 branch not support std::variant.
Please refer to example for cpp11 branch

The example builds against yaml-cpp 0.9.0 and magic_enum; yaml-cpp >= 0.7
should also work.

## use examples

```
// It relies on two other three open source libraries(yaml-cpp, magic_enum), Please refer to CMakeLists.txt of example
git clone https://github.com/fantasy-peak/yaml_cpp_struct.git
cd ./yaml_cpp_struct/example
mkdir build && cd build
cmake ..
make -j 9
./example ../config.yaml
// use xmake
cd ./yaml_cpp_struct/example
xmake build -v
xmake run example ../config.yaml
```

[Example](./example/main.cpp)
```yaml
---
flag: false
name: "fantasy-peak"
address: { "127.0.0.1": 8888, "127.0.0.2": 9999 }
msec: 100
tuple: ["fantasy-peak", 89]
map_tuple: {"fantasy-peak": ["map_tuple", 254]}
ips: ["127.0.0.1"]
```

```cpp
#include <iostream>

#include <yaml_cpp_struct.hpp>

struct AccountInfo {
    bool flag;
    std::string name;
    std::unordered_map<std::string, int> address;
    std::optional<std::string> num;
    std::chrono::milliseconds msec;
    std::tuple<std::string, uint8_t> tuple;
    std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple;
    std::string default_str;
    std::vector<std::string> ips;
};

// Bare fields are required. YCS_OPT(field, default) is used when the key is
// missing. std::optional fields must be declared explicitly.
YCS_ADD_STRUCT(AccountInfo,
    flag,
    name,
    address,
    YCS_OPT(num, std::nullopt),
    msec,
    tuple,
    map_tuple,
    YCS_OPT(default_str, "hello default"),
    ips)

// Optional: read APP_* environment variables automatically in from_yaml().
YCS_ENV_PREFIX(AccountInfo, "APP_")

int main(int, char** argv) {
    auto [cfg, error] = yaml_cpp_struct::from_yaml<AccountInfo>(argv[1]);
    if (!cfg) {
        std::cerr << error << '\n';
        return 1;
    }
    auto [str, e] = yaml_cpp_struct::to_yaml(cfg.value());
    return 0;
}
```

## Field policies

Every field must be listed in `YCS_ADD_STRUCT`:

- A bare name means **required**. If the YAML key is missing, decoding fails
  with `ErrorCode::MissingField`.
- `YCS_OPT(field, default)` means the field is optional and uses `default`
  when the key is missing.

```cpp
struct Example {
    int required_field;
    int defaulted_field;
    std::optional<int> nullable;
};

YCS_ADD_STRUCT(Example,
    required_field,
    YCS_OPT(defaulted_field, 42),
    YCS_OPT(nullable, std::nullopt))
```

Important: **in-class initializers do not participate in missing-field
decisions.** `YCS_OPT` is the single source of truth for defaults. This is
intentional, because C++ reflection cannot tell whether `int value{42};` is a
default value or just an initializer.

An explicit YAML `null` is a present value: `std::optional<T>` becomes
`std::nullopt`, while a non-optional field reports an error.

## Structured errors

The legacy tuple API is still available:

```cpp
auto [cfg, error] = yaml_cpp_struct::from_yaml<AccountInfo>("config.yaml");
```

For programmatic handling use the `Result<T>` API:

```cpp
auto result = yaml_cpp_struct::from_yaml_result<AccountInfo>("config.yaml");
if (!result) {
    std::cerr << "path=" << result.error.path
              << " line=" << result.error.line
              << " column=" << result.error.column
              << " code=" << static_cast<int>(result.error.code)
              << " message=" << result.error.message << '\n';
}

// Parse a YAML string instead of a file.
auto from_text = yaml_cpp_struct::from_yaml_str_result<AccountInfo>(yaml_text);
```

Node, stream and multi-document APIs:

```cpp
YAML::Node node = YAML::Load(text);
auto result = yaml_cpp_struct::from_node_result<AccountInfo>(node);

auto node_result = yaml_cpp_struct::to_node_result(cfg.value());
std::istringstream in(text);
auto stream_result = yaml_cpp_struct::from_stream_result<AccountInfo>(in);
yaml_cpp_struct::to_stream(cfg.value(), std::cout);

auto documents = yaml_cpp_struct::from_yaml_all_result<AccountInfo>("config.yaml");
auto documents_from_text = yaml_cpp_struct::from_yaml_all_str_result<AccountInfo>(yaml_text);
```

## Environment variables

Environment loading is enabled per struct by declaring a prefix once:

```cpp
YCS_ADD_STRUCT(AccountInfo, ...)
YCS_ENV_PREFIX(AccountInfo, "APP_")
```

`from_yaml<AccountInfo>("config.yaml")` then loads the file and overlays environment
variables automatically. `from_yaml_env` no longer exists.

Naming rules:

- prefix + upper-cased member path;
- nested fields are joined by `__`.

| YAML field | Environment variable |
|---|---|
| `flag` | `APP_FLAG=true` |
| `database.host` | `APP_DATABASE__HOST=db.internal` |
| `servers` | `APP_SERVERS='["a","b"]'` |

Precedence: `YCS_OPT` defaults < config file < environment. An environment
variable can also provide a field that is missing from the file, including a
required field.

### Nested structs

```cpp
struct Database {
    std::string host;
    int port;
};
YCS_ADD_STRUCT(Database, host, port)

struct Config {
    Database database;
    bool debug;
};
YCS_ADD_STRUCT(Config, database, debug)
YCS_ENV_PREFIX(Config, "APP_")
```

```bash
export APP_DATABASE__HOST=db.internal
export APP_DATABASE__PORT=5432
export APP_DEBUG=true
```

The whole nested value can be overridden too:

```bash
export APP_DATABASE='{host: db.internal, port: 5432}'
```

A whole-field value wins over child variables. `std::optional<Struct>` works the
same way: child variables engage the optional, and a whole-field value can set it
directly.

Explicit names on a nested struct become the base for its children:

```cpp
YCS_ADD_STRUCT(Config, YCS_ENV(database, "DB"))
// DB__HOST, DB__PORT
```

Value parsing:

- `std::string`, `std::filesystem::path` and `char` use the raw value;
- other types are parsed as YAML (`APP_SERVERS='[1,2]'`, `APP_ENABLED=true`);
- set `EnvOptions::list_separator` to accept `APP_SERVERS=1,2,3`;
- empty environment variables are ignored by default (`ignore_empty`).

Explicit field names and per-field defaults:

```cpp
YCS_ADD_STRUCT(Config,
    YCS_ENV(port, "CUSTOM_PORT"),
    YCS_ENV_OPT(timeout, "CUSTOM_TIMEOUT", 30))
```

Options and test injection:

```cpp
yaml_cpp_struct::EnvOptions options;
options.prefix = "APP_";
options.ignore_empty = false;
options.list_separator = ',';
auto result = yaml_cpp_struct::from_yaml_result<Config>("config.yaml", options);

// No config file: environment + defaults only.
auto env_only = yaml_cpp_struct::from_env_result<Config>(
    yaml_cpp_struct::env_options_for<Config>());

// Tests can use a lookup instead of the process environment.
auto lookup = [](std::string_view name) -> std::optional<std::string> { ... };
auto test_result = yaml_cpp_struct::from_yaml_result<Config>("config.yaml", lookup);
```

`EnvOptions::on_applied` is an audit callback receiving `(env_name, field_path)`;
it never receives the value. Errors use `EnvParseError`, `EnvDuplicate` and
`EnvConflict`, and messages include the environment variable name and field path,
but not the value.

Environment variables are only read when a prefix is declared (or an explicit
`EnvOptions` is passed), so unrelated process variables are never scanned.

`from_node_result` is the low-level Node decoder and does not apply environment
variables. `from_yaml_result`, `from_stream_result`, `from_yaml_all_result` and
`from_env_result` do; the `from_yaml` convenience wrapper also does.

## Enums

`YCS_ADD_ENUM` accepts both the enumerator name and its underlying integer value:

```yaml
account_type: "Personal"  # or: 1
```

## testing

The unit tests use [GoogleTest](https://github.com/google/googletest) and are a
standalone CMake project. The suite currently has **50 cases** covering parsing,
required and defaulted fields, error paths, 8-bit integers, enums, containers
(including custom comparators / hashers), round-trips, environment loading
(prefix/naming, nested and optional structs, explicit bindings, precedence,
empty values, list separators, duplicate/conflict errors, audit callbacks,
stream/multi-document merging) and the optional YAML -> JSON conversion.

```
cmake -S tests -B tests/build -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build --parallel
ctest --test-dir tests/build --output-on-failure
```
