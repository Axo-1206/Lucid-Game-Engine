#include "editor/run.h"
#include "editor/panels/console_panel.h"

#include "engine/core/log.h"

#include <algorithm>
#include <cstring>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace editor
{

    namespace
    {
        // Push each complete line in `bytes` to the buffer, and
        // return the trailing partial line (bytes after the last
        // '\n'). \r is stripped from line ends (Windows CRLF).
        std::string push_lines(ConsoleBuffer &buffer,
                               const std::string &bytes)
        {
            std::string out;
            std::size_t start = 0;
            for (std::size_t i = 0; i < bytes.size(); ++i)
            {
                if (bytes[i] != '\n')
                    continue;

                std::size_t end = i;
                if (end > start && bytes[end - 1] == '\r')
                    --end;

                if (end > start)
                {
                    buffer.push_raw(ConsoleSource::Process,
                                    engine::LogLevel::Info,
                                    std::string(),
                                    bytes.substr(start, end - start));
                }
                start = i + 1;
            }
            out.assign(bytes.begin() + static_cast<std::ptrdiff_t>(start),
                       bytes.end());
            return out;
        }
    } // namespace

#if defined(_WIN32)

    struct RunProcess::Impl
    {
        HANDLE process = nullptr;
        HANDLE stdout_read = nullptr;
        HANDLE stderr_read = nullptr;
        DWORD exit_code = 0;
    };

    RunProcess::RunProcess() : impl_(new Impl) {}
    RunProcess::~RunProcess()
    {
        stop();
        delete impl_;
    }

    bool RunProcess::start(const std::filesystem::path &executable,
                           const std::vector<std::string> &args,
                           const std::filesystem::path &working_dir,
                           std::string &error)
    {
        if (started_)
        {
            error = "RunProcess::start called twice without stop().";
            return false;
        }

        if (!std::filesystem::exists(executable))
        {
            error = "Executable not found: " + executable.string();
            return false;
        }

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;

        HANDLE out_r = nullptr, out_w = nullptr;
        HANDLE err_r = nullptr, err_w = nullptr;

        if (!CreatePipe(&out_r, &out_w, &sa, 0) ||
            !SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0))
        {
            error = "CreatePipe failed (stdout).";
            return false;
        }
        if (!CreatePipe(&err_r, &err_w, &sa, 0) ||
            !SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0))
        {
            CloseHandle(out_r);
            CloseHandle(out_w);
            error = "CreatePipe failed (stderr).";
            return false;
        }

        STARTUPINFOA si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = out_w;
        si.hStdError = err_w;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

        // Build the command line: "<exe> arg1 arg2 ...". Quoting is
        // deliberately simple; args with embedded quotes are a
        // documented limitation of phase 2.
        std::string cmd = "\"" + executable.string() + "\"";
        for (const auto &a : args)
            cmd += " \"" + a + "\"";

        const std::string cwd =
            working_dir.empty() ? std::string() : working_dir.string();
        const char *cwd_c = cwd.empty() ? nullptr : cwd.c_str();

        PROCESS_INFORMATION pi{};
        const BOOL ok = CreateProcessA(
            nullptr,
            const_cast<char *>(cmd.c_str()),
            nullptr, nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            cwd_c,
            &si,
            &pi);

        CloseHandle(out_w);
        CloseHandle(err_w);

        if (!ok)
        {
            CloseHandle(out_r);
            CloseHandle(err_r);
            error = "CreateProcess failed: " + std::to_string(GetLastError());
            return false;
        }

        CloseHandle(pi.hThread);

        impl_->process = pi.hProcess;
        impl_->stdout_read = out_r;
        impl_->stderr_read = err_r;
        pid_ = static_cast<std::uint64_t>(pi.dwProcessId);
        started_ = true;
        exited_ = false;
        exit_code_ = 0;
        partial_stdout_.clear();
        partial_stderr_.clear();
        return true;
    }

    void RunProcess::stop()
    {
        if (!started_ || !impl_ || !impl_->process)
            return;

        // If the process is still running, terminate it.
        if (WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT)
        {
            TerminateProcess(impl_->process, 0);
            WaitForSingleObject(impl_->process, 2000);
        }

        DWORD code = 0;
        GetExitCodeProcess(impl_->process, &code);
        exit_code_ = static_cast<int>(code);
        exited_ = true;

        CloseHandle(impl_->process);
        if (impl_->stdout_read)
            CloseHandle(impl_->stdout_read);
        if (impl_->stderr_read)
            CloseHandle(impl_->stderr_read);
        impl_->process = nullptr;
        impl_->stdout_read = nullptr;
        impl_->stderr_read = nullptr;
        started_ = false;
    }

    bool RunProcess::is_running() const
    {
        if (!started_ || !impl_ || !impl_->process)
            return false;
        return WaitForSingleObject(impl_->process, 0) == WAIT_TIMEOUT;
    }

    bool RunProcess::has_exited() const { return exited_ || (started_ && !is_running()); }
    int RunProcess::exit_code() const { return exit_code_; }

    void RunProcess::poll(ConsoleBuffer &buffer)
    {
        if (!started_ || !impl_ || !impl_->process)
            return;

        auto drain = [&](HANDLE h, std::string &partial)
        {
            if (!h)
                return;
            char buf[4096];
            DWORD avail = 0;
            while (PeekNamedPipe(h, nullptr, 0, nullptr, &avail, nullptr) && avail > 0)
            {
                DWORD read = 0;
                if (!ReadFile(h, buf, sizeof(buf), &read, nullptr) || read == 0)
                    break;
                partial += std::string(buf, read);
                partial = push_lines(buffer, partial);
            }
        };

        drain(impl_->stdout_read, partial_stdout_);
        drain(impl_->stderr_read, partial_stderr_);

        // Has the process exited?
        if (WaitForSingleObject(impl_->process, 0) == WAIT_OBJECT_0)
        {
            // Final drain so late output is not lost.
            drain(impl_->stdout_read, partial_stdout_);
            drain(impl_->stderr_read, partial_stderr_);

            DWORD code = 0;
            GetExitCodeProcess(impl_->process, &code);
            exit_code_ = static_cast<int>(code);
            exited_ = true;
            started_ = false;

            CloseHandle(impl_->process);
            if (impl_->stdout_read)
                CloseHandle(impl_->stdout_read);
            if (impl_->stderr_read)
                CloseHandle(impl_->stderr_read);
            impl_->process = nullptr;
            impl_->stdout_read = nullptr;
            impl_->stderr_read = nullptr;
        }
    }

#else // POSIX

    struct RunProcess::Impl
    {
        int stdout_fd = -1;
        int stderr_fd = -1;
        bool waitable = false;
    };

    RunProcess::RunProcess() : impl_(new Impl) {}
    RunProcess::~RunProcess()
    {
        stop();
        delete impl_;
    }

    bool RunProcess::start(const std::filesystem::path &executable,
                           const std::vector<std::string> &args,
                           const std::filesystem::path &working_dir,
                           std::string &error)
    {
        if (started_)
        {
            error = "RunProcess::start called twice without stop().";
            return false;
        }

        if (!std::filesystem::exists(executable))
        {
            error = "Executable not found: " + executable.string();
            return false;
        }

        int out_pipe[2] = {-1, -1};
        int err_pipe[2] = {-1, -1};
        if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0)
        {
            error = "pipe() failed.";
            return false;
        }

        const pid_t child = fork();
        if (child < 0)
        {
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(err_pipe[0]);
            close(err_pipe[1]);
            error = "fork() failed.";
            return false;
        }

        if (child == 0)
        {
            // Child. Redirect stdout/stderr, exec.
            dup2(out_pipe[1], STDOUT_FILENO);
            dup2(err_pipe[1], STDERR_FILENO);
            close(out_pipe[0]);
            close(out_pipe[1]);
            close(err_pipe[0]);
            close(err_pipe[1]);

            if (!working_dir.empty())
            {
                if (chdir(working_dir.c_str()) != 0)
                    _exit(127);
            }

            std::vector<char *> argv;
            argv.push_back(const_cast<char *>(executable.c_str()));
            for (const auto &a : args)
                argv.push_back(const_cast<char *>(a.c_str()));
            argv.push_back(nullptr);

            execv(executable.c_str(), argv.data());
            _exit(127);
        }

        // Parent.
        close(out_pipe[1]);
        close(err_pipe[1]);

        fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

        impl_->stdout_fd = out_pipe[0];
        impl_->stderr_fd = err_pipe[0];
        pid_ = static_cast<std::uint64_t>(child);
        started_ = true;
        exited_ = false;
        exit_code_ = 0;
        partial_stdout_.clear();
        partial_stderr_.clear();
        return true;
    }

    void RunProcess::stop()
    {
        if (!started_ || !impl_)
            return;

        if (is_running())
        {
            kill(static_cast<pid_t>(pid_), SIGTERM);
            // Give it a moment, then SIGKILL.
            for (int i = 0; i < 20; ++i)
            {
                int status = 0;
                const pid_t r = waitpid(static_cast<pid_t>(pid_), &status, WNOHANG);
                if (r == static_cast<pid_t>(pid_))
                {
                    exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
                    exited_ = true;
                    goto cleanup;
                }
                usleep(100 * 1000);
            }
            kill(static_cast<pid_t>(pid_), SIGKILL);
            int status = 0;
            waitpid(static_cast<pid_t>(pid_), &status, 0);
            exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            exited_ = true;
        }

    cleanup:
        if (impl_->stdout_fd >= 0)
            close(impl_->stdout_fd);
        if (impl_->stderr_fd >= 0)
            close(impl_->stderr_fd);
        impl_->stdout_fd = -1;
        impl_->stderr_fd = -1;
        started_ = false;
    }

    bool RunProcess::is_running() const
    {
        if (!started_)
            return false;
        int status = 0;
        const pid_t r = waitpid(static_cast<pid_t>(pid_), &status, WNOHANG);
        if (r == static_cast<pid_t>(pid_))
        {
            const_cast<RunProcess *>(this)->exit_code_ =
                WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            const_cast<RunProcess *>(this)->exited_ = true;
            const_cast<RunProcess *>(this)->started_ = false;
            return false;
        }
        return r == 0;
    }

    bool RunProcess::has_exited() const { return exited_ || (started_ && !is_running()); }
    int RunProcess::exit_code() const { return exit_code_; }

    void RunProcess::poll(ConsoleBuffer &buffer)
    {
        if (!started_ || !impl_)
            return;

        auto drain = [&](int fd, std::string &partial)
        {
            if (fd < 0)
                return;
            char buf[4096];
            for (;;)
            {
                const ssize_t n = read(fd, buf, sizeof(buf));
                if (n > 0)
                {
                    partial.append(buf, static_cast<std::size_t>(n));
                    partial = push_lines(buffer, partial);
                }
                else if (n == 0)
                {
                    break; // EOF
                }
                else
                {
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        break;
                    break;
                }
            }
        };

        drain(impl_->stdout_fd, partial_stdout_);
        drain(impl_->stderr_fd, partial_stderr_);

        if (!is_running())
        {
            drain(impl_->stdout_fd, partial_stdout_);
            drain(impl_->stderr_fd, partial_stderr_);
            if (impl_->stdout_fd >= 0)
                close(impl_->stdout_fd);
            if (impl_->stderr_fd >= 0)
                close(impl_->stderr_fd);
            impl_->stdout_fd = -1;
            impl_->stderr_fd = -1;
        }
    }

#endif

} // namespace editor