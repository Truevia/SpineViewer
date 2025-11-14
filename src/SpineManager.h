
#pragma once

#include <string>
#include <iostream>

#include "spine/spine.h"
#include "spine/Version.h"
#include "spine-glfw.h"

using namespace std;
using namespace spine;

class SpineManager {
public:
    bool premultipliedAlpha = true;
    float scalex = 0.3;
    float scaley = 0.3;
    uint32_t drawcall = 0;
    std::vector<std::string> animationNames;
    int currentAnimation = 0;
    bool spineLoop = true;
    float spinePosX = 0;
    float spinePosY = 0;
    float spineEntryTimeScale = 1.0;

private:
    Atlas *atlas;
    SkeletonData *skeletonData;
    Skeleton *skeleton;
    AnimationStateData *animationStateData;
    AnimationState *animationState;
    GlTextureLoader *textureLoader;
    
#if SPINE_MAJOR_VERSION == 3
    Cocos2dAtlasAttachmentLoader *_atlasAttachmentLoader = nullptr;
#endif
    
public:
    SpineManager();
    ~SpineManager();
    
    bool loadSpine(const std::string& atlasPath, const std::string& skelPath);
    
    void update(float delta);
    
    void render();
    
    void repositionSkeleton();

    void setScaleX(float x);

    void setScaleY(float y);
    
    void dispose();
    
    bool isLoaded() const;

    void setAnimationByName(const std::string& name, bool loop);

    void setX(float x);

    void setY(float y);
};
