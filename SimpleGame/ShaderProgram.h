#pragma once
#include "Dependencies/glew.h"

// Resolves shaders beside the executable, with project-directory fallbacks.
// The caller owns the returned program. Zero indicates a logged failure.
GLuint LoadShaderProgram(const char* vertexFile, const char* fragmentFile);
