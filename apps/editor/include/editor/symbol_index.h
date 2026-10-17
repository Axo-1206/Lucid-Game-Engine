#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace editor
{

    enum class SymbolKind : std::uint8_t
    {
        Import,
        Enum,
        Resource,
        Node,
    };

    const char *symbol_kind_name(SymbolKind k);

    struct SymbolEntry
    {
        std::string name;
        SymbolKind kind = SymbolKind::Node;
        std::filesystem::path file;
        std::uint32_t line = 0; // 1-based
    };

    // The regex-based symbol index. Walks the given `.lucid` files,
    // strips comments and string literals, and matches the four
    // top-level declaration forms.
    //
    // The index does not parse. It is a textual approximation, and
    // its known limitations are deliberate:
    //
    //   - It does not follow imports (a symbol from `alice.math` is
    //     not indexed unless `alice.math` is a `.lucid` file under
    //     `scripts/`).
    //   - It does not detect duplicate declarations (that is Sema's
    //     job; the Problems panel reports it).
    //   - It does not resolve qualified names (`Module::Foo`).
    //
    // Its job is navigation: given a project, find the line where a
    // name is declared. Those three limitations do not affect
    // navigation.
    class SymbolIndex
    {
    public:
        // Rebuild the index by walking `scripts/` under
        // `project_root`, recursively. Files are sorted for
        // determinism.
        //
        // Any error (missing directory, unreadable file) is logged
        // and skipped; the index holds whatever was readable.
        void rebuild(const std::filesystem::path &project_root);

        // Add or replace the entries for one file. Used by the save
        // path so a saved file's symbols appear without a full
        // rebuild.
        void refresh_file(const std::filesystem::path &file);

        void clear();

        bool empty() const { return entries_.empty(); }
        std::size_t size() const { return entries_.size(); }
        const std::vector<SymbolEntry> &entries() const { return entries_; }

        // Return every entry whose name contains `query`
        // (case-insensitive substring). Empty query returns all.
        std::vector<SymbolEntry> find(const std::string &query) const;

    private:
        std::vector<SymbolEntry> entries_;
    };

    // The cleaner and the matcher, exposed for tests.
    //
    // `strip_comments_and_strings` replaces the content of `//`
    // comments, `/* */` block comments, and string literals
    // ('"'...'"') with spaces, preserving newline positions so line
    // numbers in the cleaned text match line numbers in the
    // original. It is deliberately forgiving: an unterminated
    // block comment or string runs to end-of-file, same as the
    // library's lexer.
    std::string strip_comments_and_strings(const std::string &source);

    // Scan one (already-cleaned) source and append its declarations
    // to `out`. `file` and `source` are the inputs; `file` is
    // recorded on every entry. Line numbers are 1-based.
    void scan_declarations(const std::filesystem::path &file,
                           const std::string &cleaned_source,
                           std::vector<SymbolEntry> &out);

} // namespace editor