#include <catch2/catch_test_macros.hpp>

#include "engine/graph/graph_runtime.h"
#include "engine/nodes/node_registry.h"
#include "engine/core/log.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/scene/component_tags.h"

#include "sema/Sema.hpp"
#include "serialization/Serializer.hpp"

#include <filesystem>
#include <fstream>

using namespace engine;

namespace
{

    // Compile .lucid source to bytes and write them to `path`. Returns
    // true on success.
    bool compile_to_file(const std::string &source,
                         const std::filesystem::path &path)
    {
        auto result = lucid::sema::compile(source, "test.lucid",
                                           engine::node_registry());
        if (!result.ok)
        {
            for (const auto &d : result.diagnostics)
            {
                WARN("Sema diagnostic: " << d.message);
            }
            return false;
        }
        std::vector<std::uint8_t> bytes =
            lucid::serialization::serialize(*result.graph);
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
        {
            WARN("Failed to open " << path << " for writing");
            return false;
        }
        out.write(reinterpret_cast<const char *>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return true;
    }

    std::filesystem::path temp_dir()
    {
        return std::filesystem::temp_directory_path() / "lucid_graph_runtime_tests";
    }

    struct Fixture
    {
        engine::ecs::World world;
        Fixture()
        {
            setup_core_tables(world);
            std::filesystem::create_directories(temp_dir());
        }
        ~Fixture()
        {
            std::error_code ec;
            std::filesystem::remove_all(temp_dir(), ec);
        }
    };

} // namespace

TEST_CASE("graph_runtime: no Script table is not an error", "[graph][runtime]")
{
    Fixture f;
    graph::GraphRuntime rt(f.world, node_registry());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 0);
}

TEST_CASE("graph_runtime: loads a graph referenced by a Script component",
          "[graph][runtime]")
{
    Fixture f;

    // 1. Compile a tiny graph to .lucgraph.
    const auto lucgraph_path = temp_dir() / "test.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = EveryFrame()\n"
        "node p = PrintNode(1.0) on frame\n",
        lucgraph_path));

    // 2. Create an entity with a Script component pointing at it.
    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "test.lucgraph");

    // 3. Load the runtime with the temp dir as asset root.
    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    // 4. Tick. The PrintNode logs; we can't easily assert on the
    //    log output without a custom sink, so this test asserts the
    //    runtime doesn't crash and the graph count is correct.
    rt.tick(0.016f);
    rt.tick(0.016f);
}

TEST_CASE("graph_runtime: clear releases loaded graphs", "[graph][runtime]")
{
    Fixture f;
    const auto lucgraph_path = temp_dir() / "test.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = EveryFrame()\n"
        "node p = PrintNode(1.0) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "test.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.clear();
    REQUIRE(rt.graph_count() == 0);
}

TEST_CASE("graph_runtime: missing file does not throw", "[graph][runtime]")
{
    Fixture f;

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "does_not_exist.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE_FALSE(rt.load()); // returns false, doesn't throw
    REQUIRE(rt.graph_count() == 0);
}

namespace
{

    struct LogCapture
    {
        std::vector<std::string> messages;
        engine::LogSinkId id = 0;
        void attach()
        {
            id = engine::add_log_sink([this](const engine::LogRecord &r)
                                      { messages.push_back(r.message); });
        }
        void detach()
        {
            if (id != 0)
                engine::remove_log_sink(id);
            id = 0;
        }
        bool contains(const std::string &needle) const
        {
            for (const auto &m : messages)
            {
                if (m.find(needle) != std::string::npos)
                    return true;
            }
            return false;
        }
        std::size_t count_occurrences(const std::string &needle) const
        {
            std::size_t c = 0;
            for (const auto &m : messages)
            {
                if (m.find(needle) != std::string::npos)
                    ++c;
            }
            return c;
        }
    };

} // namespace

TEST_CASE("graph_runtime: IfNode(false) blocks its subscriber",
          "[graph][runtime]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    const auto lucgraph_path = temp_dir() / "false_gate.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = EveryFrame()\n"
        "node cond = BoolNode(false)\n"
        "node gate = IfNode(cond) on frame\n"
        "node p = PrintNode(1.0) on gate\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "false_gate.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());

    const std::size_t before = cap.messages.size();
    rt.tick(0.016f);

    // PrintNode did not run, so no new "[graph] PrintNode" log line.
    bool found_print = false;
    for (std::size_t i = before; i < cap.messages.size(); ++i)
    {
        if (cap.messages[i].find("PrintNode") != std::string::npos)
        {
            found_print = true;
            break;
        }
    }
    REQUIRE_FALSE(found_print);

    cap.detach();
    engine::install_default_log_sink();
}

TEST_CASE("graph_runtime: IfNode(true) lets its subscriber run",
          "[graph][runtime]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    const auto lucgraph_path = temp_dir() / "true_gate.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = EveryFrame()\n"
        "node cond = BoolNode(true)\n"
        "node gate = IfNode(cond) on frame\n"
        "node p = PrintNode(1.0) on gate\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "true_gate.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());

    rt.tick(0.016f);

    // PrintNode ran, so a "[graph] PrintNode" log line exists.
    REQUIRE(cap.contains("PrintNode"));

    cap.detach();
    engine::install_default_log_sink();
}

TEST_CASE("graph_runtime: a node fires at most once per tick",
          "[graph][runtime]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    // Two source triggers subscribing the same Action
    const auto lucgraph_path = temp_dir() / "multiple_parents.lucgraph";
    REQUIRE(compile_to_file(
        "node f1 = EveryFrame()\n"
        "node f2 = EveryFrame()\n"
        "node p = PrintNode(1.0) on f1, f2\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "multiple_parents.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());

    rt.tick(0.016f);

    // PrintNode should fire only once even though subscribed to both f1 and f2.
    REQUIRE(cap.count_occurrences("PrintNode") == 1);

    cap.detach();
    engine::install_default_log_sink();
}

TEST_CASE("graph_runtime: a node reads a resource field",
          "[graph][runtime][resource]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    const auto lucgraph_path = temp_dir() / "resource.lucgraph";
    REQUIRE(compile_to_file(
        "resource Config {\n"
        "    speed: float32 = 42.0,\n"
        "}\n"
        "node frame = EveryFrame()\n"
        "node p = PrintNode(Config.speed) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "resource.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);

    // PrintNode logged the resource field's value.
    REQUIRE(cap.contains("42"));

    cap.detach();
}

TEST_CASE("graph_runtime: a resource field with no written default is zero",
          "[graph][runtime][resource]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    const auto lucgraph_path = temp_dir() / "zero_resource.lucgraph";
    REQUIRE(compile_to_file(
        "resource Config {\n"
        "    speed: float32,\n"
        "}\n"
        "node frame = EveryFrame()\n"
        "node p = PrintNode(Config.speed) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "zero_resource.lucgraph");

    graph::GraphRuntime rt(f.world, node_registry());
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);

    // PrintNode logged 0.0, the type's zero.
    REQUIRE(cap.contains("0.0"));

    cap.detach();
}