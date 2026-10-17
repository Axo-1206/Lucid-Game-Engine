#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor
{

    class ConsoleBuffer; // fwd; defined in panels/console_panel.h

    // Launches a child process, captures its stdout/stderr, and
    // streams each line into a ConsoleBuffer tagged as process
    // output.
    //
    // Platform-specific: CreateProcess + anonymous pipes on Windows,
    // fork/exec + pipe + fcntl(O_NONBLOCK) on POSIX. The public API
    // is identical on both.
    //
    // Lifetime:
    //   - start() spawns the child. Exactly one start per instance;
    //     call stop() (or wait for the child to exit) before
    //     starting again.
    //   - poll() must be called once per frame while running. It
    //     reads available bytes, splits them into lines, and pushes
    //     them into the attached buffer.
    //   - stop() terminates the child (SIGTERM / TerminateProcess),
    //     waits briefly, then force-kills if needed. Idempotent.
    //   - The destructor calls stop().
    class RunProcess
    {
    public:
        RunProcess();
        ~RunProcess();

        RunProcess(const RunProcess &) = delete;
        RunProcess &operator=(const RunProcess &) = delete;

        // Spawn `executable` with `args`. The process's working
        // directory is the first arg if `working_dir` is empty and
        // args is non-empty; otherwise it is `working_dir`.
        //
        // Returns false and fills `error` on failure (executable not
        // found, spawn failed). The child is not started on failure.
        bool start(const std::filesystem::path &executable,
                   const std::vector<std::string> &args,
                   const std::filesystem::path &working_dir,
                   std::string &error);

        // Stop the child if running. Idempotent.
        void stop();

        // Read available stdout/stderr and push lines into `buffer`.
        // Call once per frame. Does nothing if not running.
        void poll(ConsoleBuffer &buffer);

        // True if the child is still running.
        bool is_running() const;

        // If the child has exited, its exit code. Meaningful only
        // when is_running() is false and has_exited() is true.
        bool has_exited() const;
        int exit_code() const;

        // The process ID, for diagnostics. 0 if not started.
        std::uint64_t pid() const { return pid_; }

    private:
        struct Impl;
        Impl *impl_ = nullptr;

        std::uint64_t pid_ = 0;
        bool started_ = false;
        bool exited_ = false;
        int exit_code_ = 0;

        // Accumulates bytes between reads so a partial line at the
        // end of a read is completed on the next poll.
        std::string partial_stdout_;
        std::string partial_stderr_;
    };

} // namespace editor