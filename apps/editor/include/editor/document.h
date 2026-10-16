#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace editor
{

    // A single open file. Owns its text buffer, its path, and its
    // dirty flag.
    //
    // A Document does not own a compiled graph. It is text. The
    // compile loop (step 9) reads from it and produces diagnostics
    // for the Problems panel; the Document is not changed by that.
    class Document
    {
    public:
        // Open an existing file. Returns nullptr and fills `error`
        // on failure.
        static std::unique_ptr<Document> open(
            const std::filesystem::path &path, std::string &error);

        // The file's absolute path. Never empty.
        const std::filesystem::path &path() const { return path_; }

        // The file's name (filename component). For display.
        std::string name() const { return path_.filename().string(); }

        // The text buffer. `mutable_text` returns a writable
        // reference for the ImGui input widget; `text` returns a
        // const reference for reading.
        const std::string &text() const { return text_; }
        std::string &mutable_text() { return text_; }

        // True if the buffer has been edited since the last save.
        bool dirty() const { return dirty_; }

        // Mark the buffer dirty. Called by the editor after the
        // input widget has changed the buffer.
        void mark_dirty() { dirty_ = true; }

        // Save the buffer to its path. Returns false and fills
        // `error` on failure.
        bool save(std::string &error);

        // Save to a different path, then update the document's
        // path. Returns false and fills `error` on failure.
        bool save_as(const std::filesystem::path &new_path,
                     std::string &error);

    private:
        Document(std::filesystem::path path, std::string text);

        std::filesystem::path path_;
        std::string text_;
        bool dirty_ = false;
    };

} // namespace editor