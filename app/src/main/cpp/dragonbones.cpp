#include "dragonbones.h"

#include "json.h"

namespace chroma {
namespace {

Transform readTransform(const json::Value& value) {
    Transform transform;
    if (!value.isObject()) {
        return transform;
    }
    transform.x = value.getFloat("x", 0.0f);
    transform.y = value.getFloat("y", 0.0f);
    transform.skX = value.getFloat("skX", 0.0f);
    transform.skY = value.getFloat("skY", 0.0f);
    transform.scX = value.getFloat("scX", 1.0f);
    transform.scY = value.getFloat("scY", 1.0f);
    return transform;
}

DisplayType readDisplayType(const json::Value& value) {
    if (value.isString()) {
        const std::string& text = value.asString();
        if (text == "image") {
            return DisplayType::Image;
        }
        if (text == "armature") {
            return DisplayType::Armature;
        }
        if (text == "mesh") {
            return DisplayType::Mesh;
        }
        if (text == "boundingBox") {
            return DisplayType::BoundingBox;
        }
        return DisplayType::Unknown;
    }
    if (value.isNumber()) {
        switch (value.asInt()) {
            case 0: return DisplayType::Image;
            case 1: return DisplayType::Armature;
            case 2: return DisplayType::Mesh;
            case 3: return DisplayType::BoundingBox;
            default: return DisplayType::Unknown;
        }
    }
    return DisplayType::Unknown;
}

AnimationData parseAnimation(const json::Value& value) {
    AnimationData animation;
    animation.name = value.getString("name");
    animation.duration = value.getInt("duration", 0);
    animation.playTimes = value.getInt("playTimes", 1);
    animation.fadeInTime = value.getFloat("fadeInTime", 0.0f);
    return animation;
}

AtlasRegion readAtlasRegion(const json::Value& value) {
    AtlasRegion region;
    region.name = value.getString("name");
    region.x = value.getInt("x", 0);
    region.y = value.getInt("y", 0);
    region.width = value.getInt("width", 0);
    region.height = value.getInt("height", 0);
    region.rotated = value.getBool("rotated", false);
    if (value.has("frameWidth") && value.has("frameHeight")) {
        region.hasFrame = true;
        region.frameX = value.getInt("frameX", 0);
        region.frameY = value.getInt("frameY", 0);
        region.frameWidth = value.getInt("frameWidth", region.width);
        region.frameHeight = value.getInt("frameHeight", region.height);
    }
    return region;
}

Aabb parseAabb(const json::Value& value) {
    Aabb aabb;
    if (!value.isObject()) {
        return aabb;
    }
    aabb.x = value.getFloat("x", 0.0f);
    aabb.y = value.getFloat("y", 0.0f);
    aabb.width = value.getFloat("width", 0.0f);
    aabb.height = value.getFloat("height", 0.0f);
    aabb.valid = aabb.width > 0.0f && aabb.height > 0.0f;
    return aabb;
}

bool parseArmature(const json::Value& value, int skeletonFrameRate, ArmatureData& out) {
    out.name = value.getString("name");
    out.frameRate = value.getInt("frameRate", skeletonFrameRate);
    out.aabb = parseAabb(value.at("aabb"));

    const json::Value& bones = value.at("bone");
    out.bones.reserve(bones.size());
    for (std::size_t i = 0; i < bones.size(); ++i) {
        const json::Value& boneValue = bones.at(i);
        BoneData bone;
        bone.name = boneValue.getString("name");
        bone.parentName = boneValue.getString("parent");
        bone.length = boneValue.getFloat("length", 0.0f);
        bone.inheritRotation = boneValue.getBool("inheritRotation", true);
        bone.inheritScale = boneValue.getBool("inheritScale", true);
        bone.inheritTranslation = boneValue.getBool("inheritTranslation", true);
        bone.inheritReflection = boneValue.getBool("inheritReflection", false);
        bone.transform = readTransform(boneValue.at("transform"));
        out.bones.push_back(std::move(bone));
    }

    for (BoneData& bone : out.bones) {
        if (bone.parentName.empty()) {
            bone.parentIndex = -1;
            continue;
        }
        bone.parentIndex = out.findBoneIndex(bone.parentName);
    }

    const json::Value& slots = value.at("slot");
    out.slots.reserve(slots.size());
    for (std::size_t i = 0; i < slots.size(); ++i) {
        const json::Value& slotValue = slots.at(i);
        SlotData slot;
        slot.name = slotValue.getString("name");
        slot.parentBone = slotValue.getString("parent");
        slot.z = slotValue.getInt("z", 0);
        slot.displayIndex = slotValue.getInt("displayIndex", 0);
        slot.alpha = slotValue.getFloat("alpha", 1.0f);
        out.slots.push_back(std::move(slot));
    }

    // Displays live on skin slots, so merge every skin into the armature slot
    // list. Skins are usually ordered; preserving that order keeps z-stable
    // drawing for duplicate slot names.
    const json::Value& skins = value.at("skin");
    for (std::size_t skinIndex = 0; skinIndex < skins.size(); ++skinIndex) {
        const json::Value& skin = skins.at(skinIndex);
        const json::Value& skinSlots = skin.at("slot");
        for (std::size_t i = 0; i < skinSlots.size(); ++i) {
            const json::Value& skinSlot = skinSlots.at(i);
            const std::string slotName = skinSlot.getString("name");
            SlotData* target = nullptr;
            for (SlotData& slot : out.slots) {
                if (slot.name == slotName) {
                    target = &slot;
                    break;
                }
            }
            if (target == nullptr) {
                SlotData slot;
                slot.name = slotName;
                out.slots.push_back(std::move(slot));
                target = &out.slots.back();
            }

            const json::Value& displays = skinSlot.at("display");
            for (std::size_t d = 0; d < displays.size(); ++d) {
                const json::Value& displayValue = displays.at(d);
                DisplayData display;
                display.type = readDisplayType(displayValue.at("type"));
                display.name = displayValue.getString("name");
                display.path = displayValue.getString("path");
                display.transform = readTransform(displayValue.at("transform"));
                const json::Value& pivot = displayValue.at("pivot");
                if (pivot.isObject()) {
                    display.pivotX = pivot.getFloat("x", 0.5f);
                    display.pivotY = pivot.getFloat("y", 0.5f);
                }
                target->displays.push_back(std::move(display));
            }
        }
    }

    const json::Value& animations = value.at("animation");
    out.animations.reserve(animations.size());
    for (std::size_t i = 0; i < animations.size(); ++i) {
        out.animations.push_back(parseAnimation(animations.at(i)));
    }
    return !out.name.empty();
}

}  // namespace

int ArmatureData::findBoneIndex(const std::string& name) const {
    for (std::size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].name == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const BoneData* ArmatureData::findBone(const std::string& name) const {
    const int index = findBoneIndex(name);
    return index >= 0 ? &bones[static_cast<std::size_t>(index)] : nullptr;
}

const SlotData* ArmatureData::findSlot(const std::string& name) const {
    for (const SlotData& slot : slots) {
        if (slot.name == name) {
            return &slot;
        }
    }
    return nullptr;
}

const AnimationData* ArmatureData::findAnimation(const std::string& name) const {
    for (const AnimationData& animation : animations) {
        if (animation.name == name) {
            return &animation;
        }
    }
    return nullptr;
}

const AtlasRegion* TextureAtlas::find(const std::string& regionName) const {
    for (const AtlasRegion& region : regions) {
        if (region.name == regionName) {
            return &region;
        }
    }
    return nullptr;
}

const ArmatureData* SkeletonData::findArmature(const std::string& name) const {
    for (const ArmatureData& armature : armatures) {
        if (armature.name == name) {
            return &armature;
        }
    }
    return nullptr;
}

bool parseSkeleton(const std::string& jsonText, SkeletonData& out, std::string* error) {
    auto document = json::Value::parse(jsonText, error);
    if (!document) {
        return false;
    }
    if (!document->isObject()) {
        if (error != nullptr) {
            *error = "skeleton root is not a JSON object";
        }
        return false;
    }

    out = SkeletonData{};
    out.name = document->getString("name");
    out.version = document->getString("version");
    out.frameRate = document->getInt("frameRate", 24);

    const json::Value& armatures = document->at("armature");
    if (!armatures.isArray() || armatures.size() == 0) {
        if (error != nullptr) {
            *error = "skeleton has no armature array";
        }
        return false;
    }

    out.armatures.reserve(armatures.size());
    for (std::size_t i = 0; i < armatures.size(); ++i) {
        ArmatureData armature;
        if (!parseArmature(armatures.at(i), out.frameRate, armature)) {
            if (error != nullptr) {
                *error = "armature " + std::to_string(i) + " has no name";
            }
            return false;
        }
        out.armatures.push_back(std::move(armature));
    }
    return true;
}

bool parseTextureAtlas(const std::string& jsonText, TextureAtlas& out, std::string* error) {
    auto document = json::Value::parse(jsonText, error);
    if (!document) {
        return false;
    }
    if (!document->isObject()) {
        if (error != nullptr) {
            *error = "texture atlas root is not a JSON object";
        }
        return false;
    }

    out = TextureAtlas{};
    out.name = document->getString("name");
    out.imagePath = document->getString("imagePath");
    out.width = document->getInt("width", 0);
    out.height = document->getInt("height", 0);

    const json::Value* subTextures = &document->at("SubTexture");
    if (!subTextures->isArray() && document->has("subTexture")) {
        // Some DragonBones exporters use a lower-case key.
        subTextures = &document->at("subTexture");
    }

    if (!subTextures->isArray()) {
        if (error != nullptr) {
            *error = "texture atlas has no SubTexture array";
        }
        return false;
    }

    out.regions.reserve(subTextures->size());
    for (std::size_t i = 0; i < subTextures->size(); ++i) {
        out.regions.push_back(readAtlasRegion(subTextures->at(i)));
    }

    if (out.regions.empty()) {
        if (error != nullptr) {
            *error = "texture atlas has no regions";
        }
        return false;
    }
    return true;
}

bool DragonBonesEngine::loadFromJson(const std::string& skeletonJson,
                                     const std::string& atlasJson,
                                     std::string* error) {
    loaded_ = false;
    error_.clear();

    SkeletonData skeleton;
    if (!parseSkeleton(skeletonJson, skeleton, &error_)) {
        if (error != nullptr) {
            *error = error_;
        }
        return false;
    }

    TextureAtlas atlas;
    if (!parseTextureAtlas(atlasJson, atlas, &error_)) {
        if (error != nullptr) {
            *error = error_;
        }
        return false;
    }

    skeleton_ = std::move(skeleton);
    atlas_ = std::move(atlas);
    loaded_ = true;
    return true;
}

}  // namespace chroma
