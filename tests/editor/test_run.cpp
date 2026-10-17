// tests/editor/test_run.cpp
//
// Step 1 — Run Action tests
//
// Three test cases, all tagged [run]:
//
//   1. "run: subprocess stdout reaches the console buffer"
//      Launches a trivial child (cmd /c echo / /bin/echo). Always
//      runnable; no game binary required. Verifies RunProcess spawns,
//      streams stdout into ConsoleBuffer, tags lines as Process, and
//      reports exit code 0.
//
//   2. "run: lucid_game --smoke-test loads a project and exits 0"
//      End-to-end test. Launches lucid_game.exe --smoke-test against
//      a temp project created via Project::create. Asserts exit 0 and
//      "Smoke test OK" in the buffer — proving the full chain:
//        argv -> resolve_launch -> read_project_file -> runtime.load()
//        -> log_info -> pipe -> ConsoleBuffer.
//      Skipped with WARN when lucid_game is not found.
//
//   3. "run: lucid_game with a project root starts and emits logs"
//      Launches lucid_game.exe without --smoke-test (as the editor's
//      on_run() does). Waits 1s, checks the buffer has *something*
//      (proving it started and is streaming), then stops it.
//      Skipped with WARN when lucid_game is not found.
//      Weaker than the smoke test by design; catches the
//      "starts but produces no output" case.

#include <catch2/catch_test_macros.hpp>

#include "editor/run.h"
#include "editor/panels/console_panel.h"
#include "editor/platform_paths.h"
#include "editor/project.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

namespace fs = std::filesystem;
using namespace editor;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

    // A temp directory unique to each test instance. Cleaned up in
    // the destructor. Same pattern as test_project.cpp / test_build.cpp.
    struct TempDir
    {
        fs::path path;

        explicit TempDir(const char *tag = "run")
        {
            path = fs::temp_directory_path() /
                   (std::string("lucid_run_test_") + tag + "_" +
                    std::to_string(reinterpret_cast<std::uintptr_t>(this)));
            fs::remove_all(path);
            fs::create_directories(path);
        }

        ~TempDir()
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    };

    // Poll `run` into `buffer` until the process exits or `timeout`
    // elapses. Returns true if the process exited before the timeout.
    bool poll_until_exit(RunProcess &run,
                         ConsoleBuffer &buffer,
                         std::chrono::milliseconds timeout)
    {
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout)
        {
            run.poll(buffer);
            if (run.has_exited())
                return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    // True if any line in the buffer contains `needle`.
    bool buffer_contains(const std::deque<ConsoleLine> &lines,
                         const std::string &needle)
    {
        for (const auto &l : lines)
            if (l.message.find(needle) != std::string::npos)
                return true;
        return false;
    }

#if defined(_WIN32)
    constexpr const char *kExeSuffix = ".exe";
#else
    constexpr const char *kExeSuffix = "";
#endif

} // namespace

// ---------------------------------------------------------------------------
// Test 1 — subprocess I/O (no game binary required)
// ---------------------------------------------------------------------------

TEST_CASE("run: subprocess stdout reaches the console buffer", "[run]")
{
    // A trivial child helper that prints a known marker string and exits 0.
    const auto exe =
        paths::executable_dir() /
        (std::string("test_helper_echo") + kExeSuffix);

    REQUIRE(fs::exists(exe));

    RunProcess run;
    ConsoleBuffer buffer;

    std::string error;
    REQUIRE(run.start(exe, {}, fs::current_path(), error));
    REQUIRE(error.empty());

    const bool exited = poll_until_exit(
        run, buffer, std::chrono::milliseconds(5000));
    REQUIRE(exited);
    REQUIRE(run.has_exited());
    REQUIRE(run.exit_code() == 0);

    const auto lines = buffer.snapshot();
    REQUIRE_FALSE(lines.empty());
    // The marker must appear in at least one line.
    REQUIRE(buffer_contains(lines, "lucid_run_test_marker"));

    // Every line from the subprocess must be tagged as process output,
    // not engine output.
    for (const auto &l : lines)
        REQUIRE(l.source == ConsoleSource::Process);
}

// ---------------------------------------------------------------------------
// Test 2 — end-to-end smoke test (requires lucid_game binary)
// ---------------------------------------------------------------------------

TEST_CASE("run: lucid_game --smoke-test loads a project and exits 0", "[run]")
{
    const auto game_exe =
        paths::executable_dir() /
        (std::string("lucid_game") + kExeSuffix);

    if (!fs::exists(game_exe))
    {
        WARN("lucid_game not found at " << game_exe.string()
             << " — skipping end-to-end Run test.");
        return;
    }

    // Build a temp project using the editor's own helper. This
    // guarantees the project layout matches what Project::create
    // produces: lucid.project.json, scenes/main.lscene, etc.
    TempDir tmp("smoke");
    const auto proj_root = tmp.path / "Proj";

    std::string error;
    auto project = Project::create(proj_root, "Proj", error);
    REQUIRE(project != nullptr);
    REQUIRE(error.empty());

    // Sanity: the active_scene file must exist before we launch.
    REQUIRE(fs::exists(proj_root / project->file().active_scene));

    // Launch: argv[1] = project root, argv[2] = --smoke-test.
    // The game should run headless (no window), load the scene, log
    // "Smoke test OK: scene=... graphs=...", and return 0.
    // Working directory: the project root so CWD-relative lookups resolve.
    RunProcess run;
    ConsoleBuffer buffer;

    REQUIRE(run.start(game_exe,
                      {project->root().string(), "--smoke-test"},
                      project->root(),
                      error));
    REQUIRE(error.empty());
    INFO("lucid_game pid: " << run.pid());

    // 15 s timeout: generous for CI cold starts.
    const bool exited = poll_until_exit(
        run, buffer, std::chrono::milliseconds(15000));

    if (!exited)
    {
        run.stop();
        FAIL("lucid_game --smoke-test did not exit within 15 seconds.");
    }

    CAPTURE(run.exit_code());
    for (const auto &l : buffer.snapshot())
        UNSCOPED_INFO("[" << l.timestamp << "] " << l.message);

    // Exit code 0 means the game loaded everything without fatal error.
    REQUIRE(run.exit_code() == 0);

    const auto lines = buffer.snapshot();

    // "Smoke test OK" proves the full path:
    //   argv -> resolve_launch -> read_project_file -> runtime.load()
    //   -> engine::log_info -> pipe -> ConsoleBuffer
    REQUIRE(buffer_contains(lines, "Smoke test OK"));

    // "scene=" comes from the formatted log_info message in main.cpp
    // and confirms active_scene was parsed out of lucid.project.json.
    REQUIRE(buffer_contains(lines, "scene="));
}

// ---------------------------------------------------------------------------
// Test 3 — starts and streams (the exact on_run() path, no --smoke-test)
// ---------------------------------------------------------------------------

TEST_CASE("run: lucid_game with a project root starts and emits logs", "[run]")
{
    const auto game_exe =
        paths::executable_dir() /
        (std::string("lucid_game") + kExeSuffix);

    if (!fs::exists(game_exe))
    {
        WARN("lucid_game not found — skipping starts-and-streams test.");
        return;
    }

    TempDir tmp("stream");
    const auto proj_root = tmp.path / "Proj";

    std::string error;
    auto project = Project::create(proj_root, "Proj", error);
    REQUIRE(project != nullptr);
    REQUIRE(error.empty());

    // Launch exactly as EditorApp::on_run() does: only argv[1], no
    // extra flags.  The game will open a window and loop; we stop it
    // after a brief wait.
    RunProcess run;
    ConsoleBuffer buffer;

    REQUIRE(run.start(game_exe,
                      {project->root().string()},
                      project->root(),
                      error));
    REQUIRE(error.empty());

    // Give it one second to start up and emit its initial log lines.
    // On a headless CI the window open may fail, but the engine still
    // logs before the window; so we'll see *something* in the buffer.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    run.poll(buffer);

    // Stop it cleanly before asserting so the process is reaped even
    // if an assert fires.
    run.stop();

    // The buffer must have at least one line: proves the process
    // started, wrote to stdout, and the pipe path works.
    const auto lines = buffer.snapshot();
    REQUIRE_FALSE(lines.empty());
}
