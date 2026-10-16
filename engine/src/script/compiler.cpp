#include "engine/script/compiler.h"
#include "engine/registry/package_manager.h"

#include "sema/Sema.hpp"
#include "serialization/Serializer.hpp"

namespace engine
{

    namespace
    {

        CompileDiagnostic::Severity translate_severity(
            lucid::diag::Severity s)
        {
            using From = lucid::diag::Severity;
            using To = CompileDiagnostic::Severity;
            switch (s)
            {
            case From::Hint:
                return To::Hint;
            case From::Note:
                return To::Note;
            case From::Warning:
                return To::Warning;
            case From::Error:
                return To::Error;
            case From::Fatal:
                return To::Fatal;
            }
            return To::Note;
        }

        CompileDiagnostic translate(const lucid::diag::Diagnostic &d)
        {
            CompileDiagnostic out;
            out.severity = translate_severity(d.severity);
            out.message = d.message;
            if (d.location.isKnown())
            {
                out.line = static_cast<std::uint32_t>(d.location.line());
                out.column = static_cast<std::uint32_t>(d.location.column());
            }
            return out;
        }

        std::vector<CompileDiagnostic> translate_all(
            const std::vector<lucid::diag::Diagnostic> &diags)
        {
            std::vector<CompileDiagnostic> out;
            out.reserve(diags.size());
            for (const auto &d : diags)
            {
                out.push_back(translate(d));
            }
            return out;
        }

        // The diagnostic for "the manager has not been applied".
        // Shared by both entry points.
        CompileDiagnostic make_not_applied_diagnostic()
        {
            CompileDiagnostic d;
            d.severity = CompileDiagnostic::Severity::Error;
            d.message = "PackageManager has not been applied; "
                        "cannot compile .lucid source.";
            return d;
        }

    } // namespace

    CompileCheckResult check_source(const std::string &source,
                                    const std::string &filename,
                                    const PackageManager &packages)
    {
        CompileCheckResult out;

        if (!packages.applied())
        {
            out.diagnostics.push_back(make_not_applied_diagnostic());
            return out;
        }

        auto result = lucid::sema::compile(source,
                                           filename,
                                           packages.registry());

        out.ok = result.ok;
        out.diagnostics = translate_all(result.diagnostics);
        return out;
    }

    CompileBuildResult compile_to_bytes(const std::string &source,
                                        const std::string &filename,
                                        const PackageManager &packages)
    {
        CompileBuildResult out;

        if (!packages.applied())
        {
            out.diagnostics.push_back(make_not_applied_diagnostic());
            return out;
        }

        auto result = lucid::sema::compile(source,
                                           filename,
                                           packages.registry());

        out.diagnostics = translate_all(result.diagnostics);
        if (!result.ok || !result.graph)
        {
            return out;
        }

        out.bytes = lucid::serialization::serialize(*result.graph);
        out.ok = true;
        return out;
    }

} // namespace engine