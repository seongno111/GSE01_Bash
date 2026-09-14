#include "stdafx.h"
#include "ShaderProgram.h"
#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace
{
    std::string ReadShader(const char* filename)
    {
        wchar_t executable[32768] = {};
        const DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
        const auto root = length > 0 && length < 32768
                              ? std::filesystem::path(executable).parent_path()
                              : std::filesystem::current_path();
        const std::filesystem::path candidates[] = {root / "Shaders" / filename,
                                                    root / "../../SimpleGame/Shaders" / filename,
                                                    root / "../SimpleGame/Shaders" / filename,
                                                    std::filesystem::path("Shaders") / filename,
                                                    std::filesystem::path("SimpleGame/Shaders")
                                                        / filename};

        for (const auto& path : candidates)
        {
            std::ifstream stream(path, std::ios::binary);
            if (stream)
            {
                std::string source(std::istreambuf_iterator<char>(stream), {});
                // Editors may add a UTF-8 BOM; GLSL requires #version first.
                if (source.compare(0, 3, "\xEF\xBB\xBF") == 0)
                {
                    source.erase(0, 3);
                }
                return source;
            }
        }

        std::cerr << "Cannot locate shader: " << filename << "\n";
        return {};
    }

    GLuint CompileShader(GLenum type, const std::string& source, const char* filename)
    {
        if (source.empty())
        {
            std::cerr << "Shader source is empty (" << filename << ").\n";
            return 0;
        }

        const GLuint shader = glCreateShader(type);
        if (!shader)
        {
            std::cerr << "Cannot create shader object (" << filename << ").\n";
            return 0;
        }

        const char* text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[4096] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "Shader compilation failed (" << filename << "): " << log << "\n";
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }
} // namespace

GLuint LoadShaderProgram(const char* vertexFile, const char* fragmentFile)
{
    const GLuint vs = CompileShader(GL_VERTEX_SHADER, ReadShader(vertexFile), vertexFile);
    const GLuint fs = CompileShader(GL_FRAGMENT_SHADER, ReadShader(fragmentFile), fragmentFile);
    if (!vs || !fs)
    {
        if (vs)
        {
            glDeleteShader(vs);
        }
        if (fs)
        {
            glDeleteShader(fs);
        }
        return 0;
    }

    const GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!program)
    {
        std::cerr << "Cannot create shader program: " << vertexFile << " / " << fragmentFile
                  << "\n";
        return 0;
    }

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[4096] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader linking failed (" << vertexFile << " / " << fragmentFile
                  << "): " << log << "\n";
        glDeleteProgram(program);
        return 0;
    }

    return program;
}
