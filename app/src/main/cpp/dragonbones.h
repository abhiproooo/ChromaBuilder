#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace chroma {

// DragonBones transform values. Rotation/skew are stored in degrees exactly as
// authored in the skeleton JSON.
struct Transform {
    float x = 0.0f;
    float y = 0.0f;
    float skX = 0.0f;
    float skY = 0.0f;
    float scX = 1.0f;
    float scY = 1.0f;
};

struct Aabb {
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

enum class DisplayType { Unknown, Image, Armature, Mesh, BoundingBox };

struct DisplayData {
    DisplayType type = DisplayType::Unknown;
    std::string name;
    std::string path;
    Transform transform;
    float pivotX = 0.5f;
    float pivotY = 0.5f;
};

struct SlotData {
    std::string name;
    std::string parentBone;
    int z = 0;
    int displayIndex = 0;
    float alpha = 1.0f;
    std::vector<DisplayData> displays;
};

struct BoneData {
    std::string name;
    std::string parentName;
    int parentIndex = -1;
    float length = 0.0f;
    bool inheritRotation = true;
    bool inheritScale = true;
    bool inheritTranslation = true;
    bool inheritReflection = false;
    Transform transform;
};

struct AnimationData {
    std::string name;
    int duration = 0;
    int playTimes = 1;
    float fadeInTime = 0.0f;
};

struct ArmatureData {
    std::string name;
    int frameRate = 24;
    Aabb aabb;
    std::vector<BoneData> bones;
    std::vector<SlotData> slots;
    std::vector<AnimationData> animations;

    int findBoneIndex(const std::string& name) const;
    const BoneData* findBone(const std::string& name) const;
    const SlotData* findSlot(const std::string& name) const;
    const AnimationData* findAnimation(const std::string& name) const;
};

struct AtlasRegion {
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool hasFrame = false;
    int frameX = 0;
    int frameY = 0;
    int frameWidth = 0;
    int frameHeight = 0;
    bool rotated = false;
};

struct TextureAtlas {
    std::string name;
    std::string imagePath;
    int width = 0;
    int height = 0;
    std::vector<AtlasRegion> regions;

    const AtlasRegion* find(const std::string& regionName) const;
};

struct SkeletonData {
    std::string name;
    std::string version;
    int frameRate = 24;
    std::vector<ArmatureData> armatures;

    const ArmatureData* findArmature(const std::string& name) const;
};

// Owns one parsed skeleton + texture atlas pair.
class DragonBonesEngine {
public:
    bool loadFromJson(const std::string& skeletonJson,
                      const std::string& atlasJson,
                      std::string* error);

    bool isLoaded() const { return loaded_; }
    const SkeletonData& skeleton() const { return skeleton_; }
    const TextureAtlas& atlas() const { return atlas_; }
    const std::string& error() const { return error_; }

private:
    SkeletonData skeleton_;
    TextureAtlas atlas_;
    bool loaded_ = false;
    std::string error_;
};

bool parseSkeleton(const std::string& jsonText, SkeletonData& out, std::string* error);
bool parseTextureAtlas(const std::string& jsonText, TextureAtlas& out, std::string* error);

}  // namespace chroma
