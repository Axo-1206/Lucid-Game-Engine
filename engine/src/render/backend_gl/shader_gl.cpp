#include "render/backend_gl/shader_gl.h"
#include "engine/log.h"

#include <glm/gtc/type_ptr.hpp>

namespace engine::gl {

namespace {

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        log_error("Shader compile failed (%s): %s",
                  type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

} // namespace

ShaderGL::~ShaderGL() {
    destroy();
}

ShaderGL::ShaderGL(ShaderGL&& other) noexcept
    : program_(other.program_)
{
    other.program_ = 0;
}

ShaderGL& ShaderGL::operator=(ShaderGL&& other) noexcept {
    if (this != &other) {
        destroy();
        program_ = other.program_;
        other.program_ = 0;
    }
    return *this;
}

bool ShaderGL::compile(const char* vertex_src, const char* fragment_src) {
    destroy();

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_src);
    if (vs == 0) return false;

    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    if (fs == 0) {
        glDeleteShader(vs);
        return false;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint success = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        log_error("Shader link failed: %s", log);
        glDeleteProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return false;
    }

    glDetachShader(prog, vs);
    glDetachShader(prog, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);

    program_ = prog;
    return true;
}

void ShaderGL::destroy() {
    if (program_ != 0) {
        glDeleteProgram(program_);
        program_ = 0;
    }
}

void ShaderGL::bind() const {
    glUseProgram(program_);
}

void ShaderGL::set_mat4(const char* name, const float* data) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) glUniformMatrix4fv(loc, 1, GL_FALSE, data);
}

void ShaderGL::set_int(const char* name, int value) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) glUniform1i(loc, value);
}

void ShaderGL::set_float(const char* name, float value) const {
    GLint loc = glGetUniformLocation(program_, name);
    if (loc >= 0) glUniform1f(loc, value);
}

} // namespace engine::gl
