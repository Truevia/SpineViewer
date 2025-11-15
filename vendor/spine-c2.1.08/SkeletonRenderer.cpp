#include "SkeletonRenderer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>

#include <spine/Atlas.h>
#include <spine/SkeletonJson.h>
#include <spine/extension.h>

using std::max;
using std::min;

namespace spine {

static const int quadTriangles[6] = {0, 1, 2, 2, 3, 0};


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
	int vertexCount = 0;
	const int* triangles = nullptr;
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
	_atlas = 0;
	debugSlots = false;
	debugBones = false;
	timeScale = 1;
	premultipliedAlpha = true;
	_position = Vec2(0.0f, 0.0f);
	_scale = Vec2(1.0f, 1.0f);
	_nodeColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	_opacity = 1.0f;
	_blendFunc = BlendFunc();
	_attributeLocations = AttributeLocations();
	_batch.reset(PolygonBatch::createWithCapacity(2000));
	assert(_batch && "Unable to allocate PolygonBatch");
	_batch->setAttributeLocations(_attributeLocations);
	_worldVertices.assign(1000, 0.0f); // Max number of vertices per mesh.
}

void SkeletonRenderer::setSkeletonData (spSkeletonData *skeletonData, bool ownsSkeletonData) {
	skeleton = spSkeleton_create(skeletonData);
	rootBone = skeleton->bones[0];
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
	if (_ownsSkeletonData) spSkeletonData_dispose(skeleton->data);
	if (_atlas) spAtlas_dispose(_atlas);
	spSkeleton_dispose(skeleton);
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
	spSkeleton_update(skeleton, deltaTime * timeScale);
}

void SkeletonRenderer::draw () {
	if (!skeleton || !_batch) return;

	skeleton->r = clamp01(_nodeColor.r);
	skeleton->g = clamp01(_nodeColor.g);
	skeleton->b = clamp01(_nodeColor.b);
	skeleton->a = clamp01(_opacity);

	auto *vertexBuffer = _worldVertices.data();

	auto buildRenderData = [this, vertexBuffer](spSlot *slot, AttachmentRenderData &out) -> bool {
		if (!slot || !slot->attachment) return false;
		switch (slot->attachment->type) {
		case SP_ATTACHMENT_REGION: {
			auto *attachment = reinterpret_cast<spRegionAttachment *>(slot->attachment);
			spRegionAttachment_computeWorldVertices(attachment, slot->bone, vertexBuffer);
			out.textureId = getTexture(attachment);
			out.uvs = attachment->uvs;
			out.vertexCount = 8;
			out.triangles = quadTriangles;
			out.triangleCount = 6;
			out.r = attachment->r;
			out.g = attachment->g;
			out.b = attachment->b;
			out.a = attachment->a;
			break;
		}
		case SP_ATTACHMENT_MESH: {
			auto *attachment = reinterpret_cast<spMeshAttachment *>(slot->attachment);
			spMeshAttachment_computeWorldVertices(attachment, slot, vertexBuffer);
			out.textureId = getTexture(attachment);
			out.uvs = attachment->uvs;
			out.vertexCount = attachment->verticesCount;
			out.triangles = attachment->triangles;
			out.triangleCount = attachment->trianglesCount;
			out.r = attachment->r;
			out.g = attachment->g;
			out.b = attachment->b;
			out.a = attachment->a;
			break;
		}
		case SP_ATTACHMENT_SKINNED_MESH: {
			auto *attachment = reinterpret_cast<spSkinnedMeshAttachment *>(slot->attachment);
			spSkinnedMeshAttachment_computeWorldVertices(attachment, slot, vertexBuffer);
			out.textureId = getTexture(attachment);
			out.uvs = attachment->uvs;
			out.vertexCount = attachment->uvsCount;
			out.triangles = attachment->triangles;
			out.triangleCount = attachment->trianglesCount;
			out.r = attachment->r;
			out.g = attachment->g;
			out.b = attachment->b;
			out.a = attachment->a;
			break;
		}
		default:
			return false;
		}
		return out.textureId != 0;
	};

	int additive = -1;
	Color color;
	for (int i = 0, n = skeleton->slotsCount; i < n; ++i) {
		spSlot *slot = skeleton->drawOrder[i];
		AttachmentRenderData renderData;
		if (!buildRenderData(slot, renderData)) continue;

		if (slot->data->additiveBlending != additive) {
			_batch->flush();
			glBlendFunc(_blendFunc.src, slot->data->additiveBlending ? GL_ONE : _blendFunc.dst);
			additive = slot->data->additiveBlending;
		}

		const float alpha = clamp01(skeleton->a * slot->a * renderData.a);
		color.a = floatToByte(alpha);
		float red = skeleton->r * slot->r * renderData.r;
		float green = skeleton->g * slot->g * renderData.g;
		float blue = skeleton->b * slot->b * renderData.b;
		if (premultipliedAlpha) {
			red *= alpha;
			green *= alpha;
			blue *= alpha;
		}
		color.r = floatToByte(red);
		color.g = floatToByte(green);
		color.b = floatToByte(blue);

		applyTransform(vertexBuffer, renderData.vertexCount);
		_batch->add(renderData.textureId, vertexBuffer, renderData.uvs, renderData.vertexCount,
			renderData.triangles, renderData.triangleCount, color);
	}
	_batch->flush();
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

GLuint SkeletonRenderer::getTexture (spSkinnedMeshAttachment* attachment) const {
	if (!attachment || !attachment->rendererObject) return 0;
	Texture* texture = textureFromRegion(reinterpret_cast<spAtlasRegion*>(attachment->rendererObject));
	return texture ? texture->id : 0;
}

// --- Convenience methods for Skeleton_* functions.

void SkeletonRenderer::updateWorldTransform () {
	spSkeleton_updateWorldTransform(skeleton);
}

void SkeletonRenderer::setToSetupPose () {
	spSkeleton_setToSetupPose(skeleton);
}
void SkeletonRenderer::setBonesToSetupPose () {
	spSkeleton_setBonesToSetupPose(skeleton);
}
void SkeletonRenderer::setSlotsToSetupPose () {
	spSkeleton_setSlotsToSetupPose(skeleton);
}

spBone* SkeletonRenderer::findBone (const char* boneName) const {
	return spSkeleton_findBone(skeleton, boneName);
}

spSlot* SkeletonRenderer::findSlot (const char* slotName) const {
	return spSkeleton_findSlot(skeleton, slotName);
}

bool SkeletonRenderer::setSkin (const char* skinName) {
	return spSkeleton_setSkinByName(skeleton, skinName) ? true : false;
}

spAttachment* SkeletonRenderer::getAttachment (const char* slotName, const char* attachmentName) const {
	return spSkeleton_getAttachmentForSlotName(skeleton, slotName, attachmentName);
}
bool SkeletonRenderer::setAttachment (const char* slotName, const char* attachmentName) {
	return spSkeleton_setAttachment(skeleton, slotName, attachmentName) ? true : false;
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

}
