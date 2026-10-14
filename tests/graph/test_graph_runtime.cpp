#include <catch2/catch_test_macros.hpp>

#include "engine/core/log.h"

#include "engine/ecs/core/component_tags.h"
#include "engine/ecs/core/entity.h"

#include "engine/graph/graph_runtime.h"
#include "engine/scene/scene.h"

#include "engine/registry/package_manager.h"
#include "engine/registry/packages/core_package.h"
#include "engine/registry/packages/logic_package.h"
#include "engine/registry/packages/math_package.h"

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
    // LogicPackage. Applied once, on first use, via
    // apply_registration_only(). All tests share it. No World is
    // involved; CorePackage and LogicPackage register no tables.

    engine::PackageManager &test_packages()
    {
        static engine::PackageManager mgr;
        static const bool loaded = [&]
        {
            mgr.load_package(std::make_unique<engine::CorePackage>());
            mgr.load_package(std::make_unique<engine::LogicPackage>());
            mgr.load_package(std::make_unique<engine::MathPackage>());
            return true;
        }();
        (void)loaded;

        static const bool applied = [&]
        {
            return mgr.apply_registration_only();
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

    // Per-test world. setup_core_tables creates the core schema and
    // an empty Engine_Script table. Tests that need a graph add a
    // Script row and call GraphRuntime::load().
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
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(1.0) on frame\n",
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
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(1.0) on frame\n",
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

TEST_CASE("graph_runtime: load is idempotent", "[graph][runtime]")
{
    Fixture f;

    const auto lucgraph_path = temp_dir() / "idempotent.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(1.0) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "idempotent.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());

    // First load: one graph.
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    // Second load: still one graph. load() clears graphs_ and reloads;
    // it does not append.
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    // Third: same.
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);
}

TEST_CASE("graph_runtime: load picks up a changed Script row",
          "[graph][runtime]")
{
    Fixture f;

    // Two graphs on disk.
    const auto graph_a = temp_dir() / "a.lucgraph";
    const auto graph_b = temp_dir() / "b.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(1.0) on frame\n",
        graph_a));
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(2.0) on frame\n",
        graph_b));

    // One entity with one Script row, pointing at A.
    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "a.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());

    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    // Change the Script row to point at B.
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "b.lucgraph");

    // Reload picks up the change.
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "b.lucgraph");
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

TEST_CASE("graph_runtime: empty Script table loads zero graphs",
          "[graph][runtime]")
{
    Fixture f;
    // setup_core_tables creates Engine_Script. It is empty.
    REQUIRE(f.world.get_table(ScriptTag::name) != nullptr);
    REQUIRE(f.world.get_table(ScriptTag::name)->count() == 0);

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());

    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 0);
}

TEST_CASE("graph_runtime: an empty Script path is skipped",
          "[graph][runtime]")
{
    Fixture f;

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    // graph column left at default "".

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());

    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 0);
}

TEST_CASE("graph_runtime: a partial load keeps the graphs that succeeded",
          "[graph][runtime]")
{
    Fixture f;

    const auto good1 = temp_dir() / "good1.lucgraph";
    const auto good2 = temp_dir() / "good2.lucgraph";
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(1.0) on frame\n",
        good1));
    REQUIRE(compile_to_file(
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(2.0) on frame\n",
        good2));

    // Three entities, three Script rows.
    {
        EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "A");
        auto s = add_component<ScriptTag>(f.world, e);
        f.world.get_table(ScriptTag::name)->set_string(s, "graph", "good1.lucgraph");
    }
    {
        EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "B");
        auto s = add_component<ScriptTag>(f.world, e);
        f.world.get_table(ScriptTag::name)->set_string(s, "graph", "missing.lucgraph");
    }
    {
        EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "C");
        auto s = add_component<ScriptTag>(f.world, e);
        f.world.get_table(ScriptTag::name)->set_string(s, "graph", "good2.lucgraph");
    }

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());

    // One of three failed; the call reports false.
    REQUIRE_FALSE(rt.load());
    // But two loaded and remain.
    REQUIRE(rt.graph_count() == 2);
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
        "node frame = Engine_EveryFrame_Node()\n"
        "node cond = Engine_Bool_Node(false)\n"
        "node gate = Engine_If_Bool_Node(cond) on frame\n"
        "node p = Engine_Print_Float_Node(1.0) on gate\n",
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
        if (cap.messages[i].find("Engine_Print_Float_Node") != std::string::npos)
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
        "node frame = Engine_EveryFrame_Node()\n"
        "node cond = Engine_Bool_Node(true)\n"
        "node gate = Engine_If_Bool_Node(cond) on frame\n"
        "node p = Engine_Print_Float_Node(1.0) on gate\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "true_gate.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());

    rt.tick(0.016f);

    REQUIRE(cap.contains("Engine_Print_Float_Node"));

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
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(Config.speed) on frame\n",
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
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(Config.speed) on frame\n",
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

TEST_CASE("graph_runtime: math nodes evaluate", "[graph][runtime][math]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    const auto lucgraph_path = temp_dir() / "math.lucgraph";
    REQUIRE(compile_to_file(
        "node a = Engine_Float_Node(2.0)\n"
        "node b = Engine_Float_Node(3.0)\n"
        "node s = Engine_Add_Float_Node(a, b)\n"
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(s) on frame\n",
        lucgraph_path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "math.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);

    REQUIRE(cap.contains("5"));

    cap.detach();
}

TEST_CASE("graph_runtime: Vector2 nodes evaluate", "[graph][runtime][math]")
{
    Fixture f;

    engine::clear_log_sinks();
    LogCapture cap;
    cap.attach();
    engine::set_log_level(engine::LogLevel::Trace);

    const auto path = temp_dir() / "vec2.lucgraph";
    REQUIRE(compile_to_file(
        "node a = Engine_Vector2_Float_Float_Node(1.0, 2.0)\n"
        "node b = Engine_Vector2_Float_Float_Node(3.0, 4.0)\n"
        "node s = Engine_Add_Vector2_Vector2_Node(a, b)\n"
        "node x = Engine_X_Vector2_Node(s)\n"
        "node frame = Engine_EveryFrame_Node()\n"
        "node p = Engine_Print_Float_Node(x) on frame\n",
        path));

    EntityRef e = create_entity(f.world, allocate_entity_id(f.world), "E");
    auto s = add_component<ScriptTag>(f.world, e);
    f.world.get_table(ScriptTag::name)->set_string(s, "graph", "vec2.lucgraph");

    engine::Services services;
    engine::graph::GraphRuntime rt(f.world, test_packages(), services);
    rt.set_asset_root(temp_dir());
    REQUIRE(rt.load());
    REQUIRE(rt.graph_count() == 1);

    rt.tick(0.016f);

    // a + b = (4, 6); x = 4.
    REQUIRE(cap.contains("4"));

    cap.detach();
}