#include "editor/document.h"

#include <fstream>
#include <sstream>

namespace editor
{

    Document::Document(std::filesystem::path path, std::string text)
        : path_(std::move(path)), text_(std::move(text))
    {
    }

    std::unique_ptr<Document> Document::open(
        const std::filesystem::path &path, std::string &error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            error = "Cannot open file: " + path.string();
            return nullptr;
        }

        std::ostringstream ss;
        ss << in.rdbuf();

        return std::unique_ptr<Document>(
            new Document(path, ss.str()));
    }

    bool Document::save(std::string &error)
    {
        std::ofstream out(path_, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "Cannot open file for writing: " + path_.string();
            return false;
        }

        out << text_;
        if (!out)
        {
            error = "Write failed: " + path_.string();
            return false;
        }

        dirty_ = false;
        return true;
    }

    bool Document::save_as(const std::filesystem::path &new_path,
                           std::string &error)
    {
        std::ofstream out(new_path, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "Cannot open file for writing: " + new_path.string();
            return false;
        }

        out << text_;
        if (!out)
        {
            error = "Write failed: " + new_path.string();
            return false;
        }

        path_ = new_path;
        dirty_ = false;
        return true;
    }

} // namespace editor