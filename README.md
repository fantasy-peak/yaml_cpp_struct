# yaml_cpp_struct

## function

It's a wrapper, It's easy to mapping yaml to cpp's struct

## use examples
```
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
content: "test-content"
map: { "hello": "world", "hello1": "world1" }
account_info:
  flag: false
  name: "xiaoli"
  address: { "127.0.0.1": 8888, "127.0.0.2": 9999 }
  msec: 100
  tuple: ["xiaoming", 89]
  map_tuple: {"xiaoxin": ["map_tuple", 254]}

vec: ["v110", "v120"]
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
    std::chrono::milliseconds msec;
    std::tuple<std::string, uint8_t> tuple;
    std::unordered_map<std::string, std::tuple<std::string, int32_t>> map_tuple;
};
YCS_ADD_STRUCT(AccountInfo, flag, name, address, num, msec, tuple, map_tuple)

int main(int, char** argv) {
    auto [cfg, error] = yaml_cpp_struct::from_yaml<Config>(argv[1]);
    auto [str, e] = yaml_cpp_struct::to_yaml(cfg.value());
    return 0;
}

```
