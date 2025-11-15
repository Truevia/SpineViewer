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
    vertices(), verticesCount(0),
    triangles(), trianglesCount(0),
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
    vertices.assign(capacity, Vertex{});
    triangles.assign(capacity * 3, 0);
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    return true;
}

PolygonBatch::~PolygonBatch () {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ibo) glDeleteBuffers(1, &ibo);
}

void PolygonBatch::setAttributeLocations (const AttributeLocations& locations) {
    attributeLocations = locations;
}

void PolygonBatch::add (GLuint addTextureId,
    const float* addVertices, const float* uvs, int addVerticesCount,
    const unsigned short* addTriangles, int addTrianglesCount,
        const Color& color, const Color* perVertexColors) {

    assert((addVerticesCount & 1) == 0 && "Vertices array must contain pairs");
    const int incomingVertices = addVerticesCount >> 1;
    const bool textureChanged = addTextureId != textureId;
    const bool capacityExceeded = (verticesCount + incomingVertices > capacity)
        || (trianglesCount + addTrianglesCount > capacity * 3);
    if (textureChanged || capacityExceeded) {
        flush();
        textureId = addTextureId;
    }

    const int baseVertex = verticesCount;
    for (int i = 0; i < incomingVertices; ++i) {
        Vertex &vertex = vertices[baseVertex + i];
        vertex.vertices.x = addVertices[i * 2];
        vertex.vertices.y = addVertices[i * 2 + 1];
        vertex.colors = perVertexColors ? perVertexColors[i] : color;
        vertex.texCoords.u = uvs[i * 2];
        vertex.texCoords.v = uvs[i * 2 + 1];
    }
    for (int i = 0; i < addTrianglesCount; ++i) {
        triangles[trianglesCount + i] = static_cast<GLushort>(static_cast<int>(addTriangles[i]) + baseVertex);
    }
    verticesCount += incomingVertices;
    trianglesCount += addTrianglesCount;
}

void PolygonBatch::flush () {
    if (!verticesCount || textureId == 0 || trianglesCount == 0 || !vao || !vbo || !ibo) return;

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verticesCount * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, trianglesCount * sizeof(GLushort), triangles.data(), GL_STREAM_DRAW);

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

    verticesCount = 0;
    trianglesCount = 0;
    textureId = 0;
}

}
