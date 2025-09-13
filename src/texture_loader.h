#pragma once
#include "gl_includes.h"
/**
 * Load a texture from a file path
 * @param path Path to the texture file (PNG, JPG, etc.)
 * @return OpenGL texture ID, or 0 if loading failed
 */
GLuint loadTexture(const char* path);