#include "SkeletonRenderer.h"

#include <algorithm>
#include <cassert>

#include <spine/Atlas.h>
#include <spine/ClippingAttachment.h>
#include <spine/SkeletonJson.h>
#include <spine/SkeletonClipping.h>
#include <spine/extension.h>

namespace spine {

static const unsigned short quadTriangles[6] = {0, 1, 2, 2, 3, 0};


namespace {

inline float clamp01 (float value) {
	if (value < 0.0f) return 0.0f;
	if (value > 1.0f) return 1.0f;
	return value;
}

inline uint8_t floatToByte (float value) {
	return static_cast<uint8_t>(clamp01(value) * 255.0f + 0.5f);
}

struct AttachmentRenderData {
	GLuint textureId = 0;
	const float* uvs = nullptr;
	int vertexCount = 0; // number of floats (x, y pairs)
	const unsigned short* triangles = nullptr;
	int triangleCount = 0;
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
};

inline Texture* textureFromRegion (spAtlasRegion* region) {
	if (!region || !region->page || !region->page->rendererObject) return nullptr;
	return static_cast<Texture*>(region->page->rendererObject);
}

}

SkeletonRenderer* SkeletonRenderer::createWithData (spSkeletonData* skeletonData, bool ownsSkeletonData) {
	return new SkeletonRenderer(skeletonData, ownsSkeletonData);
}

SkeletonRenderer* SkeletonRenderer::createWithFile (const char* skeletonDataFile, spAtlas* atlas, float scale) {
	return new SkeletonRenderer(skeletonDataFile, atlas, scale);
}

SkeletonRenderer* SkeletonRenderer::createWithFile (const char* skeletonDataFile, const char* atlasFile, float scale) {
	return new SkeletonRenderer(skeletonDataFile, atlasFile, scale);
}


void SkeletonRenderer::initialize () {
	skeleton = nullptr;
	rootBone = nullptr;
	_atlas = nullptr;
	_ownsSkeletonData = false;
	debugSlots = false;
	debugBones = false;
	timeScale = 1.0f;
	premultipliedAlpha = true;
	_position = Vec2(0.0f, 0.0f);
	_scale = Vec2(1.0f, 1.0f);
	_nodeColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	_opacity = 1.0f;
	_blendFunc = BlendFunc();
	_attributeLocations = AttributeLocations();
	_startSlotIndex = -1;
	_endSlotIndex = -1;
	_vertexEffect = nullptr;
	if (_clipper) {
		spSkeletonClipping_dispose(_clipper);
	}
	_clipper = spSkeletonClipping_create();
	_batch.reset(PolygonBatch::createWithCapacity(2000));
	assert(_batch && "Unable to allocate PolygonBatch");
	_batch->setAttributeLocations(_attributeLocations);
	_worldVertices.clear();
	_uvBuffer.clear();
	_colorBuffer.clear();
	ensureWorldVerticesCapacity(256);
}

void SkeletonRenderer::setSkeletonData (spSkeletonData *skeletonData, bool ownsSkeletonData) {
	if (!skeletonData) return;
	if (skeleton) {
		if (_ownsSkeletonData && skeleton->data) {
			spSkeletonData_dispose(skeleton->data);
		}
		spSkeleton_dispose(skeleton);
		skeleton = nullptr;
		rootBone = nullptr;
	}
	skeleton = spSkeleton_create(skeletonData);
	rootBone = (skeleton && skeleton->bonesCount > 0) ? skeleton->bones[0] : nullptr;
	_ownsSkeletonData = ownsSkeletonData;
}

SkeletonRenderer::SkeletonRenderer () {
	initialize();
}

SkeletonRenderer::SkeletonRenderer (spSkeletonData *skeletonData, bool ownsSkeletonData) {
	initialize();

	setSkeletonData(skeletonData, ownsSkeletonData);
}

SkeletonRenderer::SkeletonRenderer (const char* skeletonDataFile, spAtlas* atlas, float scale) {
	initialize();

	spSkeletonJson* json = spSkeletonJson_create(atlas);
	json->scale = scale;
	spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile);
	assert(skeletonData && "Error reading skeleton data.");
	spSkeletonJson_dispose(json);

	setSkeletonData(skeletonData, true);
}

SkeletonRenderer::SkeletonRenderer (const char* skeletonDataFile, const char* atlasFile, float scale) {
	initialize();

	_atlas = spAtlas_createFromFile(atlasFile, 0);
	assert(_atlas && "Error reading atlas file.");

	spSkeletonJson* json = spSkeletonJson_create(_atlas);
	json->scale = scale;
	spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile);
	assert(skeletonData && "Error reading skeleton data file.");
	spSkeletonJson_dispose(json);

	setSkeletonData(skeletonData, true);
}

SkeletonRenderer::~SkeletonRenderer () {
	if (skeleton) {
		if (_ownsSkeletonData && skeleton->data) spSkeletonData_dispose(skeleton->data);
		spSkeleton_dispose(skeleton);
		skeleton = nullptr;
		rootBone = nullptr;
	}
	if (_atlas) {
		spAtlas_dispose(_atlas);
		_atlas = nullptr;
	}
	if (_clipper) {
		spSkeletonClipping_dispose(_clipper);
		_clipper = nullptr;
	}
}

void SkeletonRenderer::setPosition (const Vec2& value) {
	_position = value;
}

Vec2 SkeletonRenderer::getPosition () const {
	return _position;
}

void SkeletonRenderer::setScale (const Vec2& value) {
	_scale = value;
}

float SkeletonRenderer::getScaleX () const {
	return _scale.x;
}

float SkeletonRenderer::getScaleY () const {
	return _scale.y;
}

void SkeletonRenderer::setColor (const ColorRGBA& value) {
	_nodeColor.r = clamp01(value.r);
	_nodeColor.g = clamp01(value.g);
	_nodeColor.b = clamp01(value.b);
	_nodeColor.a = clamp01(value.a);
}

ColorRGBA SkeletonRenderer::getColor () const {
	return _nodeColor;
}

void SkeletonRenderer::setOpacity (float value) {
	_opacity = clamp01(value);
}

float SkeletonRenderer::getOpacity () const {
	return _opacity;
}

void SkeletonRenderer::setBlendFunc (const BlendFunc& value) {
	_blendFunc = value;
}

BlendFunc SkeletonRenderer::getBlendFunc () const {
	return _blendFunc;
}

void SkeletonRenderer::setAttributeLocations (const AttributeLocations& locations) {
	_attributeLocations = locations;
	if (_batch) _batch->setAttributeLocations(locations);
}

const AttributeLocations& SkeletonRenderer::getAttributeLocations () const {
	return _attributeLocations;
}

void SkeletonRenderer::update (float deltaTime) {
	if (skeleton) spSkeleton_update(skeleton, deltaTime * timeScale);
}

void SkeletonRenderer::draw () {
	if (!skeleton || !_batch) return;

	const float nodeR = clamp01(_nodeColor.r);
	const float nodeG = clamp01(_nodeColor.g);
	const float nodeB = clamp01(_nodeColor.b);
	const float nodeAlpha = clamp01(_nodeColor.a) * clamp01(_opacity);
	const spColor& skeletonColor = skeleton->color;

	auto blendForMode = [this](spBlendMode blendMode) {
		BlendFunc func = _blendFunc;
		switch (blendMode) {
		case SP_BLEND_MODE_ADDITIVE:
			func.src = premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
			func.dst = GL_ONE;
			break;
		case SP_BLEND_MODE_MULTIPLY:
			func.src = GL_DST_COLOR;
			func.dst = GL_ONE_MINUS_SRC_ALPHA;
			break;
		case SP_BLEND_MODE_SCREEN:
			func.src = GL_ONE;
			func.dst = GL_ONE_MINUS_SRC_COLOR;
			break;
		default:
			func = _blendFunc;
			break;
		}
		return func;
	};

	BlendFunc currentBlend = {static_cast<GLenum>(-1), static_cast<GLenum>(-1)};
	auto ensureBlendState = [&](const BlendFunc& desired) {
		if (currentBlend.src != desired.src || currentBlend.dst != desired.dst) {
			_batch->flush();
			glBlendFunc(desired.src, desired.dst);
			currentBlend = desired;
		}
	};

	auto buildRenderData = [this](spSlot* slot, AttachmentRenderData& out) -> float* {
		if (!slot || !slot->attachment) return nullptr;
		switch (slot->attachment->type) {
		case SP_ATTACHMENT_REGION: {
			auto* attachment = reinterpret_cast<spRegionAttachment*>(slot->attachment);
			ensureWorldVerticesCapacity(8);
			float* buffer = _worldVertices.data();
			spRegionAttachment_computeWorldVertices(attachment, slot->bone, buffer, 0, 2);
			out.textureId = getTexture(attachment);
			out.uvs = attachment->uvs;
			out.vertexCount = 8;
			out.triangles = quadTriangles;
			out.triangleCount = 6;
			out.r = attachment->color.r;
			out.g = attachment->color.g;
			out.b = attachment->color.b;
			out.a = attachment->color.a;
			return buffer;
		}
		case SP_ATTACHMENT_MESH:
		case SP_ATTACHMENT_LINKED_MESH: {
			auto* attachment = reinterpret_cast<spMeshAttachment*>(slot->attachment);
			ensureWorldVerticesCapacity(attachment->super.worldVerticesLength);
			float* buffer = _worldVertices.data();
			spVertexAttachment_computeWorldVertices(SUPER(attachment), slot, 0, attachment->super.worldVerticesLength, buffer, 0, 2);
			out.textureId = getTexture(attachment);
			out.uvs = attachment->uvs;
			out.vertexCount = attachment->super.worldVerticesLength;
			out.triangles = attachment->triangles;
			out.triangleCount = attachment->trianglesCount;
			out.r = attachment->color.r;
			out.g = attachment->color.g;
			out.b = attachment->color.b;
			out.a = attachment->color.a;
			return buffer;
		}
		default:
			return nullptr;
		}
	};

	bool inRange = (_startSlotIndex == -1 && _endSlotIndex == -1);

	if (_vertexEffect) _vertexEffect->begin(_vertexEffect, skeleton);

	for (int i = 0, n = skeleton->slotsCount; i < n; ++i) {
		spSlot* slot = skeleton->drawOrder[i];

		if (_startSlotIndex >= 0 && slot->data->index == _startSlotIndex) inRange = true;
		if (!inRange) {
			spSkeletonClipping_clipEnd(_clipper, slot);
			continue;
		}
		if (_endSlotIndex >= 0 && slot->data->index == _endSlotIndex) {
			inRange = false;
		}

		spAttachment* attachment = slot->attachment;
		if (!attachment) {
			spSkeletonClipping_clipEnd(_clipper, slot);
			continue;
		}

		if (attachment->type == SP_ATTACHMENT_CLIPPING) {
			spSkeletonClipping_clipStart(_clipper, slot, reinterpret_cast<spClippingAttachment*>(attachment));
			continue;
		}

		if (slot->color.a <= 0.0f) {
			spSkeletonClipping_clipEnd(_clipper, slot);
			continue;
		}

		AttachmentRenderData renderData;
		float* vertices = buildRenderData(slot, renderData);
		if (!vertices || renderData.triangleCount <= 0 || renderData.textureId == 0) {
			spSkeletonClipping_clipEnd(_clipper, slot);
			continue;
		}

		const float* uvs = renderData.uvs;
		const unsigned short* triangles = renderData.triangles;
		int verticesLength = renderData.vertexCount;
		int triangleCount = renderData.triangleCount;

		if (spSkeletonClipping_isClipping(_clipper)) {
			spSkeletonClipping_clipTriangles(_clipper, vertices, verticesLength,
				const_cast<unsigned short*>(triangles), triangleCount,
				const_cast<float*>(uvs), 2);
			vertices = _clipper->clippedVertices->items;
			verticesLength = _clipper->clippedVertices->size;
			uvs = _clipper->clippedUVs->items;
			triangles = _clipper->clippedTriangles->items;
			triangleCount = _clipper->clippedTriangles->size;
			if (triangleCount == 0) {
				spSkeletonClipping_clipEnd(_clipper, slot);
				continue;
			}
		}

		const spColor& slotColor = slot->color;
		const float finalAlpha = clamp01(nodeAlpha * skeletonColor.a * slotColor.a * renderData.a);
		if (finalAlpha <= 0.0f) {
			spSkeletonClipping_clipEnd(_clipper, slot);
			continue;
		}
		const float colorMultiplier = premultipliedAlpha ? finalAlpha : 1.0f;
		const float finalR = clamp01(nodeR * skeletonColor.r * slotColor.r * renderData.r * colorMultiplier);
		const float finalG = clamp01(nodeG * skeletonColor.g * slotColor.g * renderData.g * colorMultiplier);
		const float finalB = clamp01(nodeB * skeletonColor.b * slotColor.b * renderData.b * colorMultiplier);

		Color packedColor;
		packedColor.r = floatToByte(finalR);
		packedColor.g = floatToByte(finalG);
		packedColor.b = floatToByte(finalB);
		packedColor.a = floatToByte(finalAlpha);

		ensureBlendState(blendForMode(slot->data->blendMode));

		const Color* perVertexColors = nullptr;
		const float* workingUVs = uvs;
		if (_vertexEffect) {
			_uvBuffer.assign(workingUVs, workingUVs + verticesLength);
			const int vertexCount = verticesLength / 2;
			_colorBuffer.resize(vertexCount);

			spColor light;
			light.r = finalR;
			light.g = finalG;
			light.b = finalB;
			light.a = finalAlpha;
			spColor dark;
			dark.r = dark.g = dark.b = 0.0f;
			dark.a = premultipliedAlpha ? finalAlpha : 0.0f;

			for (int v = 0, vi = 0; v < vertexCount; ++v, vi += 2) {
				float x = vertices[vi];
				float y = vertices[vi + 1];
				float u = _uvBuffer[vi];
				float vTex = _uvBuffer[vi + 1];
				spColor lightCopy = light;
				spColor darkCopy = dark;
				_vertexEffect->transform(_vertexEffect, &x, &y, &u, &vTex, &lightCopy, &darkCopy);
				vertices[vi] = x;
				vertices[vi + 1] = y;
				_uvBuffer[vi] = u;
				_uvBuffer[vi + 1] = vTex;

				Color vertexColor;
				vertexColor.r = floatToByte(clamp01(lightCopy.r));
				vertexColor.g = floatToByte(clamp01(lightCopy.g));
				vertexColor.b = floatToByte(clamp01(lightCopy.b));
				vertexColor.a = floatToByte(clamp01(lightCopy.a));
				_colorBuffer[v] = vertexColor;
			}

			workingUVs = _uvBuffer.data();
			perVertexColors = _colorBuffer.data();
		}

		applyTransform(vertices, verticesLength);
		_batch->add(renderData.textureId, vertices, workingUVs, verticesLength,
			triangles, triangleCount, packedColor, perVertexColors);

		spSkeletonClipping_clipEnd(_clipper, slot);
	}

	_batch->flush();
	spSkeletonClipping_clipEnd2(_clipper);
	if (_vertexEffect) _vertexEffect->end(_vertexEffect);
}

GLuint SkeletonRenderer::getTexture (spRegionAttachment* attachment) const {
	if (!attachment || !attachment->rendererObject) return 0;
	Texture* texture = textureFromRegion(reinterpret_cast<spAtlasRegion*>(attachment->rendererObject));
	return texture ? texture->id : 0;
}

GLuint SkeletonRenderer::getTexture (spMeshAttachment* attachment) const {
	if (!attachment || !attachment->rendererObject) return 0;
	Texture* texture = textureFromRegion(reinterpret_cast<spAtlasRegion*>(attachment->rendererObject));
	return texture ? texture->id : 0;
}

// --- Convenience methods for Skeleton_* functions.

void SkeletonRenderer::updateWorldTransform () {
	if (skeleton) spSkeleton_updateWorldTransform(skeleton);
}

void SkeletonRenderer::setToSetupPose () {
	if (skeleton) spSkeleton_setToSetupPose(skeleton);
}
void SkeletonRenderer::setBonesToSetupPose () {
	if (skeleton) spSkeleton_setBonesToSetupPose(skeleton);
}
void SkeletonRenderer::setSlotsToSetupPose () {
	if (skeleton) spSkeleton_setSlotsToSetupPose(skeleton);
}

spBone* SkeletonRenderer::findBone (const char* boneName) const {
	return skeleton ? spSkeleton_findBone(skeleton, boneName) : nullptr;
}

spSlot* SkeletonRenderer::findSlot (const char* slotName) const {
	return skeleton ? spSkeleton_findSlot(skeleton, slotName) : nullptr;
}

bool SkeletonRenderer::setSkin (const char* skinName) {
	return skeleton ? (spSkeleton_setSkinByName(skeleton, skinName) ? true : false) : false;
}

spAttachment* SkeletonRenderer::getAttachment (const char* slotName, const char* attachmentName) const {
	return skeleton ? spSkeleton_getAttachmentForSlotName(skeleton, slotName, attachmentName) : nullptr;
}
bool SkeletonRenderer::setAttachment (const char* slotName, const char* attachmentName) {
	if (!skeleton) return false;
	return spSkeleton_setAttachment(skeleton, slotName, attachmentName) ? true : false;
}

void SkeletonRenderer::setVertexEffect (spVertexEffect* effect) {
	_vertexEffect = effect;
}

spVertexEffect* SkeletonRenderer::getVertexEffect () const {
	return _vertexEffect;
}

void SkeletonRenderer::setSlotsRange (int startSlotIndex, int endSlotIndex) {
	if (startSlotIndex < 0 || endSlotIndex < 0) {
		_startSlotIndex = -1;
		_endSlotIndex = -1;
		return;
	}
	_startSlotIndex = startSlotIndex;
	_endSlotIndex = endSlotIndex;
}

void SkeletonRenderer::setOpacityModifyRGB (bool value) {
	premultipliedAlpha = value;
}

bool SkeletonRenderer::isOpacityModifyRGB () const {
	return premultipliedAlpha;
}

void SkeletonRenderer::applyTransform (float* vertices, int verticesCount) const {
	const float scaleX = _scale.x;
	const float scaleY = _scale.y;
	const float translateX = _position.x;
	const float translateY = _position.y;
	for (int i = 0; i < verticesCount; i += 2) {
		vertices[i] = vertices[i] * scaleX + translateX;
		vertices[i + 1] = vertices[i + 1] * scaleY + translateY;
	}
}

void SkeletonRenderer::ensureWorldVerticesCapacity (size_t floatsCount) {
	if (_worldVertices.size() < floatsCount) {
		_worldVertices.resize(floatsCount, 0.0f);
	}
}

}
