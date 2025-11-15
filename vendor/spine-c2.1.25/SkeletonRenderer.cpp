#include "SkeletonRenderer.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cfloat>

#include <glad/glad.h>

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
	atlas = 0;
	debugSlots = false;
	debugBones = false;
	timeScale = 1;
	premultipliedAlpha = true;
	position = Vec2(0.0f, 0.0f);
	scale = Vec2(1.0f, 1.0f);
	nodeColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
	opacity = 1.0f;
	blendFunc = BlendFunc();
	attributeLocations = AttributeLocations();
	batch.reset(PolygonBatch::createWithCapacity(2000));
	assert(batch && "Unable to allocate PolygonBatch");
	batch->setAttributeLocations(attributeLocations);
	worldVertices = MALLOC(float, 1000); // Max number of vertices per mesh.
}

void SkeletonRenderer::setSkeletonData (spSkeletonData *skeletonData, bool ownsSkeletonData) {
	skeleton = spSkeleton_create(skeletonData);
	rootBone = skeleton->bones[0];
	this->ownsSkeletonData = ownsSkeletonData;
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

	atlas = spAtlas_createFromFile(atlasFile, 0);
	assert(atlas && "Error reading atlas file.");

	spSkeletonJson* json = spSkeletonJson_create(atlas);
	json->scale = scale;
	spSkeletonData* skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonDataFile);
	assert(skeletonData && "Error reading skeleton data file.");
	spSkeletonJson_dispose(json);

	setSkeletonData(skeletonData, true);
}

SkeletonRenderer::~SkeletonRenderer () {
	if (ownsSkeletonData) spSkeletonData_dispose(skeleton->data);
	if (atlas) spAtlas_dispose(atlas);
	spSkeleton_dispose(skeleton);
	FREE(worldVertices);
}

void SkeletonRenderer::setPosition (const Vec2& value) {
	position = value;
}

Vec2 SkeletonRenderer::getPosition () const {
	return position;
}

void SkeletonRenderer::setScale (const Vec2& value) {
	scale = value;
}

float SkeletonRenderer::getScaleX () const {
	return scale.x;
}

float SkeletonRenderer::getScaleY () const {
	return scale.y;
}

void SkeletonRenderer::setColor (const ColorRGBA& value) {
	nodeColor.r = clamp01(value.r);
	nodeColor.g = clamp01(value.g);
	nodeColor.b = clamp01(value.b);
	nodeColor.a = clamp01(value.a);
}

ColorRGBA SkeletonRenderer::getColor () const {
	return nodeColor;
}

void SkeletonRenderer::setOpacity (float value) {
	opacity = clamp01(value);
}

float SkeletonRenderer::getOpacity () const {
	return opacity;
}

void SkeletonRenderer::setBlendFunc (const BlendFunc& value) {
	blendFunc = value;
}

BlendFunc SkeletonRenderer::getBlendFunc () const {
	return blendFunc;
}

void SkeletonRenderer::setAttributeLocations (const AttributeLocations& locations) {
	attributeLocations = locations;
	if (batch) batch->setAttributeLocations(locations);
}

const AttributeLocations& SkeletonRenderer::getAttributeLocations () const {
	return attributeLocations;
}

void SkeletonRenderer::update (float deltaTime) {
	spSkeleton_update(skeleton, deltaTime * timeScale);
}

void SkeletonRenderer::draw () {
	if (!skeleton || !batch) return;

	skeleton->r = clamp01(nodeColor.r);
	skeleton->g = clamp01(nodeColor.g);
	skeleton->b = clamp01(nodeColor.b);
	skeleton->a = clamp01(opacity);

	int additive = -1;
	Color color;
	const float* uvs = nullptr;
	int verticesCount = 0;
	const int* triangles = nullptr;
	int trianglesCount = 0;
	float r = 0, g = 0, b = 0, a = 0;
	for (int i = 0, n = skeleton->slotsCount; i < n; i++) {
		spSlot* slot = skeleton->drawOrder[i];
		if (!slot->attachment) continue;
		GLuint textureId = 0;
		switch (slot->attachment->type) {
		case SP_ATTACHMENT_REGION: {
			spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
			spRegionAttachment_computeWorldVertices(attachment, slot->bone, worldVertices);
			textureId = getTexture(attachment);
			uvs = attachment->uvs;
			verticesCount = 8;
			triangles = quadTriangles;
			trianglesCount = 6;
			r = attachment->r;
			g = attachment->g;
			b = attachment->b;
			a = attachment->a;
			break;
		}
		case SP_ATTACHMENT_MESH: {
			spMeshAttachment* attachment = (spMeshAttachment*)slot->attachment;
			spMeshAttachment_computeWorldVertices(attachment, slot, worldVertices);
			textureId = getTexture(attachment);
			uvs = attachment->uvs;
			verticesCount = attachment->verticesCount;
			triangles = attachment->triangles;
			trianglesCount = attachment->trianglesCount;
			r = attachment->r;
			g = attachment->g;
			b = attachment->b;
			a = attachment->a;
			break;
		}
		case SP_ATTACHMENT_SKINNED_MESH: {
			spSkinnedMeshAttachment* attachment = (spSkinnedMeshAttachment*)slot->attachment;
			spSkinnedMeshAttachment_computeWorldVertices(attachment, slot, worldVertices);
			textureId = getTexture(attachment);
			uvs = attachment->uvs;
			verticesCount = attachment->uvsCount;
			triangles = attachment->triangles;
			trianglesCount = attachment->trianglesCount;
			r = attachment->r;
			g = attachment->g;
			b = attachment->b;
			a = attachment->a;
			break;
		}
		}
		if (textureId) {
			if (slot->data->additiveBlending != additive) {
				batch->flush();
				glBlendFunc(blendFunc.src, slot->data->additiveBlending ? GL_ONE : blendFunc.dst);
				additive = slot->data->additiveBlending;
			}
			float alpha = clamp01(skeleton->a * slot->a * a);
			color.a = floatToByte(alpha);
			float red = skeleton->r * slot->r * r;
			float green = skeleton->g * slot->g * g;
			float blue = skeleton->b * slot->b * b;
			if (premultipliedAlpha) {
				red *= alpha;
				green *= alpha;
				blue *= alpha;
			}
			color.r = floatToByte(red);
			color.g = floatToByte(green);
			color.b = floatToByte(blue);
			applyTransform(worldVertices, verticesCount);
			batch->add(textureId, worldVertices, uvs, verticesCount, triangles, trianglesCount, color);
		}
	}
	batch->flush();
}

GLuint SkeletonRenderer::getTexture (spRegionAttachment* attachment) const {
	if (!attachment || !attachment->rendererObject) return 0;
	spAtlasRegion* region = (spAtlasRegion*)attachment->rendererObject;
	if (!region->page || !region->page->rendererObject) return 0;
	Texture* texture = static_cast<Texture*>(region->page->rendererObject);
	return texture ? texture->id : 0;
}

GLuint SkeletonRenderer::getTexture (spMeshAttachment* attachment) const {
	if (!attachment || !attachment->rendererObject) return 0;
	spAtlasRegion* region = (spAtlasRegion*)attachment->rendererObject;
	if (!region->page || !region->page->rendererObject) return 0;
	Texture* texture = static_cast<Texture*>(region->page->rendererObject);
	return texture ? texture->id : 0;
}

GLuint SkeletonRenderer::getTexture (spSkinnedMeshAttachment* attachment) const {
	if (!attachment || !attachment->rendererObject) return 0;
	spAtlasRegion* region = (spAtlasRegion*)attachment->rendererObject;
	if (!region->page || !region->page->rendererObject) return 0;
	Texture* texture = static_cast<Texture*>(region->page->rendererObject);
	return texture ? texture->id : 0;
}


BoundingBox SkeletonRenderer::boundingBox () const {
	BoundingBox box;
	if (!skeleton) return box;
	float minX = FLT_MAX, minY = FLT_MAX, maxX = -FLT_MAX, maxY = -FLT_MAX;
	for (int i = 0; i < skeleton->slotsCount; ++i) {
		spSlot* slot = skeleton->slots[i];
		if (!slot->attachment) continue;
		int verticesCount;
		if (slot->attachment->type == SP_ATTACHMENT_REGION) {
			spRegionAttachment* attachment = (spRegionAttachment*)slot->attachment;
			spRegionAttachment_computeWorldVertices(attachment, slot->bone, worldVertices);
			verticesCount = 8;
		} else if (slot->attachment->type == SP_ATTACHMENT_MESH) {
			spMeshAttachment* mesh = (spMeshAttachment*)slot->attachment;
			spMeshAttachment_computeWorldVertices(mesh, slot, worldVertices);
			verticesCount = mesh->verticesCount;
		} else if (slot->attachment->type == SP_ATTACHMENT_SKINNED_MESH) {
			spSkinnedMeshAttachment* mesh = (spSkinnedMeshAttachment*)slot->attachment;
			spSkinnedMeshAttachment_computeWorldVertices(mesh, slot, worldVertices);
			verticesCount = mesh->uvsCount;
		} else
			continue;
		applyTransform(worldVertices, verticesCount);
		for (int ii = 0; ii < verticesCount; ii += 2) {
			float x = worldVertices[ii];
			float y = worldVertices[ii + 1];
			minX = min(minX, x);
			minY = min(minY, y);
			maxX = max(maxX, x);
			maxY = max(maxY, y);
		}
	}
	if (minX == FLT_MAX) return box;
	box.x = minX;
	box.y = minY;
	box.width = maxX - minX;
	box.height = maxY - minY;
	return box;
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
	const float scaleX = scale.x;
	const float scaleY = scale.y;
	const float translateX = position.x;
	const float translateY = position.y;
	for (int i = 0; i < verticesCount; i += 2) {
		vertices[i] = vertices[i] * scaleX + translateX;
		vertices[i + 1] = vertices[i + 1] * scaleY + translateY;
	}
}

}
