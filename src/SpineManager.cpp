
#include "SpineManager.h"

#include <string>
#include <iostream>

using namespace std;
using namespace spine;

extern int width, height;
extern renderer_t *g_renderer;

SpineManager::SpineManager() : atlas(nullptr), skeletonData(nullptr), skeleton(nullptr), 
                    animationStateData(nullptr), animationState(nullptr), textureLoader(nullptr)
{
    textureLoader = new GlTextureLoader();
}
    
SpineManager::~SpineManager() {
    dispose();
    if (textureLoader) {
        delete textureLoader;
        textureLoader = nullptr;
    }
}
    
bool SpineManager::loadSpine(const std::string& atlasPath, const std::string& skelPath) {
    // Dispose previous resources
    dispose();
        
    // Load the atlas and the skeleton data
    atlas = new Atlas(atlasPath.c_str(), textureLoader);
    if (!atlas) {
        std::cerr << "Failed to load atlas: " << atlasPath << std::endl;
        dispose();
        return false;
    }

#if SPINE_MAJOR_VERSION == 3
    _atlasAttachmentLoader = new (__FILE__, __LINE__) Cocos2dAtlasAttachmentLoader(atlas);
#endif
    /* .json / .skel */
    if (skelPath.find(".json") != std::string::npos) {
#if SPINE_MAJOR_VERSION == 3
        SkeletonJson json(_atlasAttachmentLoader);
#else
        SkeletonJson json(atlas);
#endif
        skeletonData = json.readSkeletonDataFile(skelPath.c_str());
    } else {
#if SPINE_MAJOR_VERSION == 3
        SkeletonBinary binary(_atlasAttachmentLoader);
#else
        SkeletonBinary binary(atlas);
#endif
        skeletonData = binary.readSkeletonDataFile(skelPath.c_str());
    }
    if (!skeletonData) {
        std::cerr << "Failed to load skeleton data: " << skelPath << std::endl;
        dispose();
        return false;
    }
    
    // Create a skeleton from the data
    skeleton = new Skeleton(skeletonData);
    if (!skeleton) {
        std::cerr << "Failed to create skeleton" << std::endl;
        dispose();
        return false;
    }
    
    if (spinePosX == 0 && spinePosY == 0) {
        spinePosX = width / 2;
        spinePosY = height - 100;
    }
    currentAnimation = 0;
    animationNames.clear();
    skeleton->setPosition(spinePosX, spinePosY);
    skeleton->setScaleX(scalex);
    skeleton->setScaleY(scaley);

    auto &animations = skeletonData->getAnimations();
    std::cout << "Loaded " << animations.size() << " animations:" << std::endl;
    for (int i = 0; i < animations.size(); i++)
    {
        auto anim = animations[i];
        std::cout << " - " << anim->getName().buffer() << " (duration:" << anim->getDuration() << "s)" << std::endl;
        animationNames.push_back(anim->getName().buffer());
    }
    
    // Create animation state
    animationStateData = new AnimationStateData(skeletonData);
    if (!animationStateData) {
        std::cerr << "Failed to create animation state data" << std::endl;
        dispose();
        return false;
    }
    
    animationStateData->setDefaultMix(0.2f);
    animationState = new AnimationState(animationStateData);
    if (!animationState) {
        std::cerr << "Failed to create animation state" << std::endl;
        dispose();
        return false;
    }
    
    // Set default animation if available
    if (skeletonData->getAnimations().size() > 0) {
        Animation *firstAnim = skeletonData->getAnimations()[currentAnimation];
        // animationState->setAnimation(0, firstAnim->getName(), spineLoop);
        setAnimationByName(firstAnim->getName().buffer(), spineLoop);
    }
    
    return true;
}
    
void SpineManager::update(float delta) {
    if (animationState && skeleton) {
        animationState->update(delta);
        animationState->apply(*skeleton);
        skeleton->update(delta);
#if SPINE_MAJOR_VERSION >= 4
       skeleton->updateWorldTransform(spine::Physics_Update);
#else
        skeleton->updateWorldTransform();
#endif

        spinePosX = skeleton->getX();
        spinePosY = skeleton->getY();
    }
}
    
void SpineManager::render() {
    if (skeleton && g_renderer) {
        drawcall = renderer_draw(g_renderer, skeleton, premultipliedAlpha);
    }
}
    
void SpineManager::repositionSkeleton() {
    if (skeleton) {
        skeleton->setPosition(width / 2, height - 100);
    }
}

void SpineManager::setScaleX(float x)
{
    scalex = x;
    if (skeleton) {
        skeleton->setScaleX(x);
    }
}

void SpineManager::setScaleY(float y)
{
    scaley = y;
    if (skeleton) 
    {
        skeleton->setScaleY(y);
    }
}
    
void SpineManager::dispose()
{
    if (animationState) {
        delete animationState;
        animationState = nullptr;
    }
    if (animationStateData) {
        delete animationStateData;
        animationStateData = nullptr;
    }
    if (skeleton) {
        delete skeleton;
        skeleton = nullptr;
    }
    if (skeletonData) {
        delete skeletonData;
        skeletonData = nullptr;
    }
    if (atlas) {
        delete atlas;
        atlas = nullptr;
    }
}
    
bool SpineManager::isLoaded() const
{
    return skeleton != nullptr;
}

void SpineManager::setAnimationByName(const std::string& name, bool loop)
{
    if (animationState && skeletonData) {
        Animation* animation = skeletonData->findAnimation(name.c_str());
        if (animation) {
            TrackEntry *entry = animationState->setAnimation(0, name.c_str(), loop);
            entry->setTimeScale(spineEntryTimeScale);

        } else {
            std::cerr << "Animation not found: " << name << std::endl;
        }
    }
}

void SpineManager::setX(float x)
{
    if (skeleton) {
        skeleton->setX(x);
    }
}

void SpineManager::setY(float y)
{
    if (skeleton) {
        skeleton->setY(y);
    }
}
