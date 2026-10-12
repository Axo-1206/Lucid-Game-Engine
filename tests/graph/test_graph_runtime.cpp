#include <catch2/catch_test_macros.hpp>

#include "engine/graph/graph_runtime.h"
#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include "engine/scene/component_tags.h"
#include "engine/core/log.h"

#include "sema/Sema.hpp"
#include "serialization/Serializer.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace engine;

namespace
{

    // -----------------------------------------------------------------------------
    // Test packages
    // -----------------------------------------------------------------------------
    //
    // A process-lifetime PackageManager holding CorePackage and
    // LogicPackage. Applied once, on first use, against a throwaway
    // World (CorePackage and LogicPackage register no tables). All
    // tests share it.

    engine::PackageManager &test_packages()
    {
        static engine::PackageManager mgr;
        static const bool loaded = [&]
        {
            mgr.load_package(std::make_unique<engine::CorePackage>());
            mgr.load_package(std::make_unique<engine::LogicPackage>());
            return true;
        }();
        (void)loaded;

        static const bool applied = [&]
        {
            static engine::ecs::World scratch;
            return mgr.apply(scratch);
        }();
        (void)applied;

        return mgr;
    }

    // -----------------------------------------------------------------------------
    // Log capture
    // -----------------------------------------------------------------------------

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
    };

    // -----------------------------------------------------------------------------
    // Test files
    // -----------------------------------------------------------------------------

    bool compile_to_file(const std::string &source,
                         const std::filesystem::path &path)
    {
        auto result = lucid::sema::compile(source, "test.lucid",
                                           test_packages().registry());
        if (!result.ok)
        {
            for (const auto &d : result.diagnostics)
            {
                WARN("Sema: " << d.message);
            }
            return false;
        }
        auto bytes = lucid::serialization::serialize(*result.graph);
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open())
            return false;
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

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("graph_runtime: no Script table is not an error", "[graph][runtime]")
{
    Fixture f;
    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 0);
}

TEST_CASE("graph_runtime: loads a graph referenced by a Script component",
          "[graph][runtime]")
{
    Fixture f;

    const auto lucgraph_path = temp_dir() / "test.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(1.0) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "test.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);
    rt.tick(0.016f);
}

TEST_CASE("graph_runtime: clear releases loaded graphs", "[graph][runtime]")
{
    Fixture f;

    const auto lucgraph_path = temp_dir() / "test.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(1.0) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "test.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
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

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE_FALSE(rt.load());
    REQUIRE(rt.graph_count() == 0);
}

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
        "node frame = Engine_EveryFrame()\n"
        "node cond = Engine_BoolNode(false)\n"
        "node gate = Engine_IfNode(cond) on frame\n"
        "node p = Engine_PrintNode(1.0) on gate\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "false_gate.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());

    const std::size_t before = cap.messages.size();
    rt.tick(0.016f);

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
        "node frame = Engine_EveryFrame()\n"
        "node cond = Engine_BoolNode(true)\n"
        "node gate = Engine_IfNode(cond) on frame\n"
        "node p = Engine_PrintNode(1.0) on gate\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "true_gate.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());

    rt.tick(0.016f);

    REQUIRE(cap.contains("PrintNode"));

    cap.detach();
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
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(Config.speed) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "resource.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);

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
        "node frame = Engine_EveryFrame()\n"
        "node p = Engine_PrintNode(Config.speed) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "zero_resource.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);

    REQUIRE(cap.contains("0.0"));

    cap.detach();
}