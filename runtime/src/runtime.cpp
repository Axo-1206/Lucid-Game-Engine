#include "lucid/runtime.h"

#include <stdexcept>
#include <utility>

namespace lucid {

Runtime::Runtime() = default;
Runtime::~Runtime() = default;

Table* Runtime::create_table(const std::string& name) {
    if (tables_.count(name)) throw std::runtime_error("Table already exists: " + name);
    auto table = std::make_unique<Table>(name);
    Table* raw = table.get();
    tables_[name] = std::move(table);
    table_order_.push_back(name);
    return raw;
}

Table* Runtime::get_table(const std::string& name) {
    auto it = tables_.find(name);
    return it == tables_.end() ? nullptr : it->second.get();
}

const Table* Runtime::get_table(const std::string& name) const {
    auto it = tables_.find(name);
    return it == tables_.end() ? nullptr : it->second.get();
}

bool Runtime::has_table(const std::string& name) const {
    return tables_.count(name) > 0;
}

void Runtime::register_host_fn(const std::string& name, HostFn fn) {
    host_fns_[name] = std::move(fn);
}

bool Runtime::has_host_fn(const std::string& name) const {
    return host_fns_.count(name) > 0;
}

CellValue Runtime::call_host_fn(const std::string& name, const std::vector<CellValue>& args) {
    auto it = host_fns_.find(name);
    if (it == host_fns_.end()) throw std::runtime_error("No such host function: " + name);
    return it->second(args);
}

} // namespace lucid
