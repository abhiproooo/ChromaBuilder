#include "renderer.h"

#include <cmath>
#include <cstddef>
#include <functional>

namespace chroma {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kPiQuarter = kPi * 0.25f;
constexpr float kDegreeToRadian = kPi / 180.0f;
constexpr int kMaxArmatureDepth = 16;

float safeAtan(float value) {
    return std::atan(value);
}

Matrix2D transformMatrixData(const Transform& transform) {
    BoneTransform result;
    result.x = transform.x;
    result.y = transform.y;
    result.rotation = transform.skY * kDegreeToRadian;
    result.skew = (transform.skX - transform.skY) * kDegreeToRadian;
    result.scaleX = transform.scX;
    result.scaleY = transform.scY;
    return result.toMatrix();
}

}  // namespace

Matrix2D Matrix2D::concat(const Matrix2D& parent) const {
    Matrix2D result;
    result.a = a * parent.a + b * parent.c;
    result.b = a * parent.b + b * parent.d;
    result.c = c * parent.a + d * parent.c;
    result.d = c * parent.b + d * parent.d;
    result.tx = tx * parent.a + ty * parent.c + parent.tx;
    result.ty = tx * parent.b + ty * parent.d + parent.ty;
    return result;
}

void Matrix2D::transformPoint(float x, float y, float& outX, float& outY) const {
    outX = a * x + c * y + tx;
    outY = b * x + d * y + ty;
}

Matrix2D BoneTransform::toMatrix() const {
    Matrix2D matrix;
    if (rotation == 0.0f) {
        matrix.a = 1.0f;
        matrix.b = 0.0f;
    } else {
        matrix.a = std::cos(rotation);
        matrix.b = std::sin(rotation);
    }

    if (skew == 0.0f) {
        matrix.c = -matrix.b;
        matrix.d = matrix.a;
    } else {
        matrix.c = -std::sin(skew + rotation);
        matrix.d = std::cos(skew + rotation);
    }

    if (scaleX != 1.0f) {
        matrix.a *= scaleX;
        matrix.b *= scaleX;
    }
    if (scaleY != 1.0f) {
        matrix.c *= scaleY;
        matrix.d *= scaleY;
    }

    matrix.tx = x;
    matrix.ty = y;
    return matrix;
}

void BoneTransform::fromMatrix(const Matrix2D& matrix, float backupScaleX, float backupScaleY) {
    x = matrix.tx;
    y = matrix.ty;

    rotation = matrix.a == 0.0f
        ? (matrix.b >= 0.0f ? kPi * 0.5f : -kPi * 0.5f)
        : safeAtan(matrix.b / matrix.a);
    float skewX = matrix.d == 0.0f
        ? (matrix.c <= 0.0f ? kPi * 0.5f : -kPi * 0.5f)
        : safeAtan(-matrix.c / matrix.d);

    scaleX = (rotation > -kPiQuarter && rotation < kPiQuarter)
        ? matrix.a / std::cos(rotation)
        : matrix.b / std::sin(rotation);
    scaleY = (skewX > -kPiQuarter && skewX < kPiQuarter)
        ? matrix.d / std::cos(skewX)
        : -matrix.c / std::sin(skewX);

    if (backupScaleX >= 0.0f && scaleX < 0.0f) {
        scaleX = -scaleX;
        rotation -= kPi;
    }
    if (backupScaleY >= 0.0f && scaleY < 0.0f) {
        scaleY = -scaleY;
        skewX -= kPi;
    }

    skew = skewX - rotation;
}

BoneTransform boneTransformFromData(const Transform& transform) {
    BoneTransform result;
    result.x = transform.x;
    result.y = transform.y;
    result.rotation = transform.skY * kDegreeToRadian;
    result.skew = (transform.skX - transform.skY) * kDegreeToRadian;
    result.scaleX = transform.scX;
    result.scaleY = transform.scY;
    return result;
}

void DragonBonesRenderer::setBoneOffset(const std::string& boneName, float x, float y) {
    boneOffsets_[boneName] = {x, y};
}

void DragonBonesRenderer::clearBoneOffsets() {
    boneOffsets_.clear();
}

bool DragonBonesRenderer::buildFrame(const SkeletonData& skeleton,
                                     const TextureAtlas& atlas,
                                     int armatureIndex,
                                     std::vector<SpriteCommand>& out) {
    out.clear();
    if (armatureIndex < 0 ||
        static_cast<std::size_t>(armatureIndex) >= skeleton.armatures.size()) {
        return false;
    }

    renderArmature(skeleton,
                   atlas,
                   skeleton.armatures[static_cast<std::size_t>(armatureIndex)],
                   Matrix2D{},
                   0,
                   out);
    return true;
}

void DragonBonesRenderer::renderArmature(const SkeletonData& skeleton,
                                         const TextureAtlas& atlas,
                                         const ArmatureData& armature,
                                         const Matrix2D& parentMatrix,
                                         int depth,
                                         std::vector<SpriteCommand>& out) {
    if (depth > kMaxArmatureDepth || armature.bones.empty()) {
        return;
    }

    const std::size_t boneCount = armature.bones.size();
    std::vector<Matrix2D> boneMatrices(boneCount);
    std::vector<BoneTransform> boneGlobals(boneCount);
    std::vector<unsigned char> states(boneCount, 0);

    std::function<void(std::size_t)> computeBone = [&](std::size_t index) {
        if (index >= boneCount || states[index] == 2) {
            return;
        }
        if (states[index] == 1) {
            return;
        }
        states[index] = 1;

        const BoneData& bone = armature.bones[index];
        if (bone.parentIndex >= 0) {
            computeBone(static_cast<std::size_t>(bone.parentIndex));
        }

        BoneTransform local = boneTransformFromData(bone.transform);
        const auto offset = boneOffsets_.find(bone.name);
        if (offset != boneOffsets_.end()) {
            local.x += offset->second.first;
            local.y += offset->second.second;
        }

        BoneTransform global = local;
        Matrix2D matrix;

        if (bone.parentIndex < 0) {
            matrix = local.toMatrix();
        } else {
            const std::size_t parentIndex = static_cast<std::size_t>(bone.parentIndex);
            const Matrix2D& parent = boneMatrices[parentIndex];
            const BoneTransform& parentGlobal = boneGlobals[parentIndex];

            if (bone.inheritScale) {
                if (!bone.inheritRotation) {
                    global.rotation = global.rotation - parentGlobal.rotation;
                }
                matrix = global.toMatrix();
                matrix = matrix.concat(parent);
                if (bone.inheritTranslation) {
                    global.x = matrix.tx;
                    global.y = matrix.ty;
                } else {
                    matrix.tx = global.x;
                    matrix.ty = global.y;
                }
            } else {
                if (bone.inheritTranslation) {
                    const float x = global.x;
                    const float y = global.y;
                    global.x = parent.a * x + parent.c * y + parent.tx;
                    global.y = parent.b * x + parent.d * y + parent.ty;
                }

                if (bone.inheritRotation) {
                    float rotation = global.rotation + parentGlobal.rotation;
                    if (parentGlobal.scaleX < 0.0f) {
                        rotation += kPi;
                    }
                    const float determinant = parent.a * parent.d - parent.b * parent.c;
                    if (determinant < 0.0f) {
                        rotation -= global.rotation * 2.0f;
                        if (bone.inheritReflection) {
                            global.skew += kPi;
                        }
                    }
                    global.rotation = rotation;
                }

                matrix = global.toMatrix();
            }
        }

        global.fromMatrix(matrix, local.scaleX, local.scaleY);
        boneMatrices[index] = matrix;
        boneGlobals[index] = global;
        states[index] = 2;
    };

    for (std::size_t i = 0; i < boneCount; ++i) {
        computeBone(i);
    }

    for (const SlotData& slot : armature.slots) {
        const int parentIndex = armature.findBoneIndex(slot.parentBone);
        if (parentIndex < 0) {
            continue;
        }
        if (slot.displayIndex < 0 ||
            static_cast<std::size_t>(slot.displayIndex) >= slot.displays.size()) {
            continue;
        }

        const DisplayData& display =
            slot.displays[static_cast<std::size_t>(slot.displayIndex)];
        const Matrix2D& boneMatrix = boneMatrices[static_cast<std::size_t>(parentIndex)];
        const Matrix2D displayMatrix = transformMatrixData(display.transform)
                                           .concat(boneMatrix)
                                           .concat(parentMatrix);

        if (display.type == DisplayType::Image) {
            const std::string& regionName =
                display.path.empty() ? display.name : display.path;
            const AtlasRegion* region = atlas.find(regionName);
            if (region == nullptr || region->rotated || region->width <= 0 ||
                region->height <= 0) {
                continue;
            }

            const float originalWidth =
                region->hasFrame ? static_cast<float>(region->frameWidth)
                                 : static_cast<float>(region->width);
            const float originalHeight =
                region->hasFrame ? static_cast<float>(region->frameHeight)
                                 : static_cast<float>(region->height);
            const float frameX =
                region->hasFrame ? static_cast<float>(region->frameX) : 0.0f;
            const float frameY =
                region->hasFrame ? static_cast<float>(region->frameY) : 0.0f;

            const float pivotX = display.pivotX * originalWidth + frameX;
            const float pivotY = display.pivotY * originalHeight + frameY;

            SpriteCommand command;
            command.atlasX = region->x;
            command.atlasY = region->y;
            command.atlasWidth = region->width;
            command.atlasHeight = region->height;
            command.x = -pivotX;
            command.y = -pivotY;
            command.width = static_cast<float>(region->width);
            command.height = static_cast<float>(region->height);
            command.a = displayMatrix.a;
            command.b = displayMatrix.b;
            command.c = displayMatrix.c;
            command.d = displayMatrix.d;
            command.tx = displayMatrix.tx;
            command.ty = displayMatrix.ty;
            command.alpha = slot.alpha;
            out.push_back(command);
        } else if (display.type == DisplayType::Armature) {
            const std::string& childName = display.path.empty() ? display.name : display.path;
            const ArmatureData* child = skeleton.findArmature(childName);
            if (child != nullptr) {
                renderArmature(skeleton, atlas, *child, displayMatrix, depth + 1, out);
            }
        }
    }
}

}  // namespace chroma
