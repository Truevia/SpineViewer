#include "PolygonBatch.h"
#include <cassert>
#include <cstdlib>

namespace spine {

PolygonBatch* PolygonBatch::createWithCapacity (int capacity) {
    PolygonBatch* batch = new PolygonBatch();
    if (!batch->initWithCapacity(capacity)) {
        delete batch;
        return nullptr;
    }
    return batch;
}

PolygonBatch::PolygonBatch () :
    capacity(0),
    vertices(nullptr), verticesCount(0),
    triangles(nullptr), trianglesCount(0),
    textureId(0),
    attributeLocations(),
    vao(0),
    vbo(0),
    ibo(0)
{}

bool PolygonBatch::initWithCapacity (int capacity) {
    // 32767 is max index, so 32767 / 3 - (32767 / 3 % 3) = 10920.
    assert(capacity <= 10920 && "capacity cannot be > 10920");
    assert(capacity >= 0 && "capacity cannot be < 0");
    this->capacity = capacity;
    vertices = (Vertex*)malloc(sizeof(Vertex) * capacity);
    triangles = (GLushort*)malloc(sizeof(GLushort) * capacity * 3);
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    return true;
}

PolygonBatch::~PolygonBatch () {
    free(vertices);
    free(triangles);
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ibo) glDeleteBuffers(1, &ibo);
}

void PolygonBatch::setAttributeLocations (const AttributeLocations& locations) {
    attributeLocations = locations;
}

void PolygonBatch::add (GLuint addTextureId,
        const float* addVertices, const float* uvs, int addVerticesCount,
        const int* addTriangles, int addTrianglesCount,
        const Color& color) {

    if (
        addTextureId != textureId
        || verticesCount + (addVerticesCount >> 1) > capacity
        || trianglesCount + addTrianglesCount > capacity * 3) {
        this->flush();
        textureId = addTextureId;
    }

    for (int i = 0; i < addTrianglesCount; ++i, ++trianglesCount)
        triangles[trianglesCount] = addTriangles[i] + verticesCount;

    for (int i = 0; i < addVerticesCount; i += 2, ++verticesCount) {
        Vertex* vertex = vertices + verticesCount;
        vertex->vertices.x = addVertices[i];
        vertex->vertices.y = addVertices[i + 1];
        vertex->colors = color;
        vertex->texCoords.u = uvs[i];
        vertex->texCoords.v = uvs[i + 1];
    }
}

void PolygonBatch::flush () {
    if (!verticesCount || textureId == 0 || !vao || !vbo || !ibo) return;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verticesCount * sizeof(Vertex), vertices, GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, trianglesCount * sizeof(GLushort), triangles, GL_STREAM_DRAW);

    const GLsizei stride = sizeof(Vertex);
    glEnableVertexAttribArray(attributeLocations.position);
    glVertexAttribPointer(attributeLocations.position, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, vertices));
    glEnableVertexAttribArray(attributeLocations.color);
    glVertexAttribPointer(attributeLocations.color, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (const void*)offsetof(Vertex, colors));
    glEnableVertexAttribArray(attributeLocations.texCoords);
    glVertexAttribPointer(attributeLocations.texCoords, 2, GL_FLOAT, GL_FALSE, stride, (const void*)offsetof(Vertex, texCoords));

    glDrawElements(GL_TRIANGLES, trianglesCount, GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(attributeLocations.position);
    glDisableVertexAttribArray(attributeLocations.color);
    glDisableVertexAttribArray(attributeLocations.texCoords);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    verticesCount = 0;
    trianglesCount = 0;
    textureId = 0;
}

}
