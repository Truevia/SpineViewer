#pragma once

#include <memory>
#include <vector>

#include <spine/spine.h>
#include "PolygonBatch.h"

namespace spine {

struct Texture {
	GLuint id = 0;
	int width = 0;
	int height = 0;
};

struct BlendFunc {
	GLenum src = GL_ONE;
	GLenum dst = GL_ONE_MINUS_SRC_ALPHA;
};

struct Vec2 {
	float x = 0.0f;
	float y = 0.0f;
	Vec2() = default;
	Vec2(float xVal, float yVal) : x(xVal), y(yVal) {}
};

struct ColorRGBA {
	float r = 1.0f;
	float g = 1.0f;
	float b = 1.0f;
	float a = 1.0f;
	ColorRGBA() = default;
	ColorRGBA(float rVal, float gVal, float bVal, float aVal)
		: r(rVal), g(gVal), b(bVal), a(aVal) {}
};

struct BoundingBox {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

/** Draws a skeleton using raw OpenGL. */
class SkeletonRenderer {
public:
	spSkeleton* skeleton;
	spBone* rootBone;
	float timeScale;
	bool debugSlots;
	bool debugBones;
	bool premultipliedAlpha;

	static SkeletonRenderer* createWithData (spSkeletonData* skeletonData, bool ownsSkeletonData = false);
	static SkeletonRenderer* createWithFile (const char* skeletonDataFile, spAtlas* atlas, float scale = 0);
	static SkeletonRenderer* createWithFile (const char* skeletonDataFile, const char* atlasFile, float scale = 0);

	SkeletonRenderer (spSkeletonData* skeletonData, bool ownsSkeletonData = false);
	SkeletonRenderer (const char* skeletonDataFile, spAtlas* atlas, float scale = 0);
	SkeletonRenderer (const char* skeletonDataFile, const char* atlasFile, float scale = 0);

	virtual ~SkeletonRenderer ();

	virtual void update (float deltaTime);
	virtual void draw ();
	virtual BoundingBox boundingBox () const;

	void setPosition (const Vec2& position);
	Vec2 getPosition () const;
	void setScale (const Vec2& scale);
	float getScaleX () const;
	float getScaleY () const;
	void setColor (const ColorRGBA& color);
	ColorRGBA getColor () const;
	void setOpacity (float opacity);
	float getOpacity () const;
	void setBlendFunc (const BlendFunc& blend);
	BlendFunc getBlendFunc () const;
	void setAttributeLocations (const AttributeLocations& locations);
	const AttributeLocations& getAttributeLocations () const;

	// --- Convenience methods for common Skeleton_* functions.
	void updateWorldTransform ();

	void setToSetupPose ();
	void setBonesToSetupPose ();
	void setSlotsToSetupPose ();

	/* Returns 0 if the bone was not found. */
	spBone* findBone (const char* boneName) const;
	/* Returns 0 if the slot was not found. */
	spSlot* findSlot (const char* slotName) const;
	
	/* Sets the skin used to look up attachments not found in the SkeletonData defaultSkin. Attachments from the new skin are
	 * attached if the corresponding attachment from the old skin was attached. If there was no old skin, each slot's setup mode
	 * attachment is attached from the new skin. Returns false if the skin was not found.
	 * @param skin May be 0.*/
	bool setSkin (const char* skinName);

	/* Returns 0 if the slot or attachment was not found. */
	spAttachment* getAttachment (const char* slotName, const char* attachmentName) const;
	/* Returns false if the slot or attachment was not found. */
	bool setAttachment (const char* slotName, const char* attachmentName);

	void setOpacityModifyRGB (bool value);
	bool isOpacityModifyRGB () const;

protected:
	SkeletonRenderer ();
	void setSkeletonData (spSkeletonData* skeletonData, bool ownsSkeletonData);
	GLuint getTexture (spRegionAttachment* attachment) const;
	GLuint getTexture (spMeshAttachment* attachment) const;
	GLuint getTexture (spSkinnedMeshAttachment* attachment) const;

private:
	bool ownsSkeletonData;
	spAtlas* atlas;
	std::unique_ptr<PolygonBatch> batch;
	AttributeLocations attributeLocations;
	float* worldVertices;
	Vec2 position;
	Vec2 scale;
	ColorRGBA nodeColor;
	float opacity;
	BlendFunc blendFunc;
	mutable std::vector<float> debugVertices;
	void initialize ();
	void applyTransform (float* vertices, int verticesCount) const;
};

}
