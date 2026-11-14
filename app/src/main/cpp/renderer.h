#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "dragonbones.h"

namespace chroma {

struct Matrix2D {
    float a = 1.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;

    Matrix2D concat(const Matrix2D& parent) const;
    void transformPoint(float x, float y, float& outX, float& outY) const;
};

struct BoneTransform {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    float skew = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;

    Matrix2D toMatrix() const;
    void fromMatrix(const Matrix2D& matrix, float backupScaleX, float backupScaleY);
};

BoneTransform boneTransformFromData(const Transform& transform);

struct SpriteCommand {
    int atlasX = 0;
    int atlasY = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float a = 1.0f;
    float b = 0.0f;
    float c = 0.0f;
    float d = 1.0f;
    float tx = 0.0f;
    float ty = 0.0f;
    float alpha = 1.0f;
};

class DragonBonesRenderer {
public:
    void setBoneOffset(const std::string& boneName, float x, float y);
    void clearBoneOffsets();

    bool buildFrame(const SkeletonData& skeleton,
                    const TextureAtlas& atlas,
                    int armatureIndex,
                    std::vector<SpriteCommand>& out);

private:
    void renderArmature(const SkeletonData& skeleton,
                        const TextureAtlas& atlas,
                        const ArmatureData& armature,
                        const Matrix2D& parentMatrix,
                        int depth,
                        std::vector<SpriteCommand>& out);

    std::unordered_map<std::string, std::pair<float, float>> boneOffsets_;
};

}  // namespace chroma
