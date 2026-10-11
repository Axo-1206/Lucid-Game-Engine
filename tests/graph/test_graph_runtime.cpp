#include <catch2/catch_test_macros.hpp>

#include "engine/graph/graph_runtime.h"
#include "engine/nodes/node_registry.h"
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