# yaml_cpp_struct

[![](https://github.com/fantasy-peak/yaml_cpp_struct/workflows/test/badge.svg)](https://github.com/fantasy-peak/yaml_cpp_struct/actions)

## function

It's a wrapper, It's easy to mapping yaml to cpp's struct

## note

main branch need >= c++17, if you need to use c++11, please use cpp11 branch. cpp11 branch not support std::variant.
Please refer to example for cpp11 branch

## use examples
```
// It relies on two other three open source libraries(yaml-cpp, visit_struct, magic_enum), Please refer to CMakeLists.txt of example
git clone https://github.com/fantasy-peak/yaml_cpp_struct.git
cd ypp/example
mkdir build && cd build
cmake ..
make -j 9
./example ../config.yaml
```


[Example](./example/main.cpp)
```yaml
---
ch: 'A'
price: 100.05
count: 254
content: "i'm fantasy-peak"
map: { "fantasy": "good", "peak": "good" }
account_info:
  flag: false
  name: "fantasy-peak"
  address: { "127.0.0.1": 8888, "127.0.0.2": 9999 }
  msec: 100
  tuple: ["fantasy-peak", 89]
  map_tuple: {"fantasy-peak": ["map_tuple", 254]}

vec: ["fantasy-001", "fantasy-002"]
set_vec: [9, 5, 7]
account_type: "Personal"
```

```cpp
#include <iostream>

#include <spdlog/spdlog.h>
#include <yaml_cpp_struct.hpp>

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
    std::chrono::milliseconds msec{100};
    std::tuple<std::string, uint8_t> tuple;
    std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple;
    std::string default_str{"hello default"};
};
YCS_ADD_STRUCT(AccountInfo, flag, name, address, num, msec, tuple, map_tuple, default_str)

int main(int, char** argv) {
    auto [cfg, error] = yaml_cpp_struct::from_yaml<Config>(argv[1]);
    auto [str, e] = yaml_cpp_struct::to_yaml(cfg.value());
    return 0;
}

```
