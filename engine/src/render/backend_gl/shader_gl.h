#pragma once

#include <glad/glad.h>

#include <string>

namespace engine::gl {

// A compiled and linked OpenGL shader program.
class ShaderGL {
public:
    ShaderGL() = default;
    ~ShaderGL();

    ShaderGL(const ShaderGL&) = delete;
    ShaderGL& operator=(const ShaderGL&) = delete;
    ShaderGL(ShaderGL&&) noexcept;
    ShaderGL& operator=(ShaderGL&&) noexcept;

    // Compile and link. Returns false on failure and logs the errors.
    bool compile(const char* vertex_src, const char* fragment_src);

    // Delete the program.
    void destroy();

    // Bind for use.
    void bind() const;

    // Uniform setters.
    void set_mat4(const char* name, const float* data) const;
    void set_int(const char* name, int value) const;
    void set_float(const char* name, float value) const;

    bool is_valid() const { return program_ != 0; }
    GLuint program() const { return program_; }

private:
    GLuint program_ = 0;
};

} // namespace engine::gl
