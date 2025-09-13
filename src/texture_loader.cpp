#include "texture_loader.h"
#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

GLuint loadTexture(const char* path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);
    
    if (!data) {
        fprintf(stderr, "Failed to load texture: %s\n", path);
        //print internal error from stb_image
        fprintf(stderr, "stb_image error: %s\n", stbi_failure_reason());
        return 0;
    }
    
    printf("Loaded texture: %dx%d with %d channels\n", width, height, channels);
    
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    // Upload texture data
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    stbi_image_free(data);
    return textureID;
}