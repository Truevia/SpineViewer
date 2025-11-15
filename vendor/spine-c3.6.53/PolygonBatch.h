#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "glad/glad.h"

namespace spine {

struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct Vertex {
    struct {
        float x;
        float y;
    } vertices;
    Color colors;
    struct {
        float u;
        float v;
    } texCoords;
};

struct AttributeLocations {
    GLint position = 0;
    GLint color = 1;
    GLint texCoords = 2;
};

class PolygonBatch {
public:
    static PolygonBatch* createWithCapacity (int capacity);

    PolygonBatch();

    virtual ~PolygonBatch();

    bool initWithCapacity (int capacity);
    void setAttributeLocations (const AttributeLocations& locations);
    void add (GLuint textureId,
        const float* vertices, const float* uvs, int verticesCount,
        const unsigned short* triangles, int trianglesCount,
        const Color& color, const Color* perVertexColors = nullptr);
    void flush ();

private:
    int capacity;
    std::vector<Vertex> vertices;
    int verticesCount;
    std::vector<GLushort> triangles;
    int trianglesCount;
    GLuint textureId;
    AttributeLocations attributeLocations;
    GLuint vao;
    GLuint vbo;
    GLuint ibo;
};

};


