#pragma once

#include "lucid/cellvalue.h"
#include "lucid/table.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace lucid {

class Runtime {
public:
    Runtime();
    ~Runtime();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    Table* create_table(const std::string& name);
    Table* get_table(const std::string& name);
    const Table* get_table(const std::string& name) const;
    bool has_table(const std::string& name) const;

    template <typename Fn>
    void each_table(Fn&& fn) {
        for (auto& name : table_order_) {
            fn(*tables_.at(name));
        }
    }

    using HostFn = std::function<CellValue(const std::vector<CellValue>&)>;

    void register_host_fn(const std::string& name, HostFn fn);
    bool has_host_fn(const std::string& name) const;
    CellValue call_host_fn(const std::string& name, const std::vector<CellValue>& args);

private:
    std::unordered_map<std::string, std::unique_ptr<Table>> tables_;
    std::unordered_map<std::string, HostFn> host_fns_;
    std::vector<std::string> table_order_;
};

} // namespace lucid
