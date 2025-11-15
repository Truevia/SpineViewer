#include "spine-opengl.h"

#include <spine/extension.h>
#include <glad/glad.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

struct SpineTexture {
    GLuint id;
    int width;
    int height;
};

static SpineTexture* createTexture(const char* path) {
    int width = 0, height = 0, comp = 0;
    stbi_uc* pixels = stbi_load(path, &width, &height, &comp, STBI_rgb_alpha);
    if (!pixels) {
        printf("Failed to load texture: %s", path);
        return nullptr;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    auto* handle = new SpineTexture{tex, width, height};
    return handle;
}

void _spAtlasPage_createTexture(spAtlasPage* self, const char* path) {
    SpineTexture* texture = createTexture(path);
    if (!texture) return;
    self->rendererObject = texture;
    self->width = texture->width;
    self->height = texture->height;
}

void _spAtlasPage_disposeTexture(spAtlasPage* self) {
    if (!self || !self->rendererObject) return;
    auto* texture = reinterpret_cast<SpineTexture*>(self->rendererObject);
    if (texture->id) glDeleteTextures(1, &texture->id);
    delete texture;
    self->rendererObject = nullptr;
}

char* _spUtil_readFile(const char* path, int* length) {
    FILE* file = fopen(path, "rb");
    if (!file) return nullptr;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* data = static_cast<char*>(malloc(size));
    if (!data) {
        fclose(file);
        return nullptr;
    }

    size_t read = fread(data, 1, size, file);
    fclose(file);

    if (read != static_cast<size_t>(size)) {
        free(data);
        return nullptr;
    }

    *length = static_cast<int>(size);
    return data;
}
