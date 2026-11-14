#include <jni.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>

#include <string>
#include <vector>

#include "dragonbones.h"
#include "renderer.h"

namespace {

constexpr char kLogTag[] = "ChromaNative";

chroma::DragonBonesEngine gEngine;
chroma::DragonBonesRenderer gRenderer;
std::string gLastError;

std::string fromJavaString(JNIEnv* env, jstring value) {
    if (value == nullptr) {
        return {};
    }
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) {
        return {};
    }
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

jobjectArray toJavaStringArray(JNIEnv* env, const std::vector<std::string>& values) {
    jclass stringClass = env->FindClass("java/lang/String");
    jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()),
                                             stringClass, nullptr);
    if (array == nullptr) {
        return nullptr;
    }
    for (std::size_t i = 0; i < values.size(); ++i) {
        jstring item = env->NewStringUTF(values[i].c_str());
        env->SetObjectArrayElement(array, static_cast<jsize>(i), item);
        env->DeleteLocalRef(item);
    }
    return array;
}

jfloatArray toJavaFloatArray(JNIEnv* env, const std::vector<float>& values) {
    jfloatArray array = env->NewFloatArray(static_cast<jsize>(values.size()));
    if (array == nullptr) {
        return nullptr;
    }
    if (!values.empty()) {
        env->SetFloatArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
    }
    return array;
}

bool readAsset(AAssetManager* manager, const std::string& path, std::string& out) {
    AAsset* asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_STREAMING);
    if (asset == nullptr) {
        return false;
    }

    const off_t length = AAsset_getLength(asset);
    if (length < 0) {
        AAsset_close(asset);
        return false;
    }

    out.resize(static_cast<std::size_t>(length));
    std::size_t offset = 0;
    while (offset < out.size()) {
        const int bytes = AAsset_read(asset, out.data() + offset, out.size() - offset);
        if (bytes <= 0) {
            break;
        }
        offset += static_cast<std::size_t>(bytes);
    }
    AAsset_close(asset);
    out.resize(offset);
    return offset == static_cast<std::size_t>(length);
}

const chroma::ArmatureData* armatureAt(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= gEngine.skeleton().armatures.size()) {
        return nullptr;
    }
    return &gEngine.skeleton().armatures[static_cast<std::size_t>(index)];
}

}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_rebuilt_MainActivity_getEngineVersion(JNIEnv* env, jobject /*thiz*/) {
    return env->NewStringUTF("ChromaToons Native Engine 2.1.0 (DragonBones renderer)");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_example_rebuilt_MainActivity_loadDragonBones(JNIEnv* env, jobject /*thiz*/,
                                                      jobject assetManager,
                                                      jstring skeletonPath,
                                                      jstring atlasPath) {
    AAssetManager* manager = AAssetManager_fromJava(env, assetManager);
    if (manager == nullptr) {
        gLastError = "AssetManager is null";
        return JNI_FALSE;
    }

    const std::string skeletonFile = fromJavaString(env, skeletonPath);
    const std::string atlasFile = fromJavaString(env, atlasPath);

    std::string skeletonJson;
    if (!readAsset(manager, skeletonFile, skeletonJson)) {
        gLastError = "Cannot read asset: " + skeletonFile;
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", gLastError.c_str());
        return JNI_FALSE;
    }

    std::string atlasJson;
    if (!readAsset(manager, atlasFile, atlasJson)) {
        gLastError = "Cannot read asset: " + atlasFile;
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", gLastError.c_str());
        return JNI_FALSE;
    }

    std::string error;
    if (!gEngine.loadFromJson(skeletonJson, atlasJson, &error)) {
        gLastError = error.empty() ? "DragonBones parse failed" : error;
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "%s", gLastError.c_str());
        return JNI_FALSE;
    }

    gLastError.clear();
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "Loaded skeleton '%s': %zu armatures, atlas '%s': %zu regions",
                        gEngine.skeleton().name.c_str(),
                        gEngine.skeleton().armatures.size(),
                        gEngine.atlas().name.c_str(),
                        gEngine.atlas().regions.size());
    return JNI_TRUE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_rebuilt_MainActivity_getLoadError(JNIEnv* env, jobject /*thiz*/) {
    return env->NewStringUTF(gLastError.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_rebuilt_MainActivity_getSkeletonName(JNIEnv* env, jobject /*thiz*/) {
    return env->NewStringUTF(gEngine.skeleton().name.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_rebuilt_MainActivity_getSkeletonFrameRate(JNIEnv* /*env*/, jobject /*thiz*/) {
    return static_cast<jint>(gEngine.skeleton().frameRate);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_example_rebuilt_MainActivity_getArmatureNames(JNIEnv* env, jobject /*thiz*/) {
    std::vector<std::string> names;
    names.reserve(gEngine.skeleton().armatures.size());
    for (const chroma::ArmatureData& armature : gEngine.skeleton().armatures) {
        names.push_back(armature.name);
    }
    return toJavaStringArray(env, names);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_example_rebuilt_MainActivity_getBoneNames(JNIEnv* env, jobject /*thiz*/,
                                                   jint armatureIndex) {
    const chroma::ArmatureData* armature = armatureAt(armatureIndex);
    if (armature == nullptr) {
        return toJavaStringArray(env, {});
    }
    std::vector<std::string> names;
    names.reserve(armature->bones.size());
    for (const chroma::BoneData& bone : armature->bones) {
        names.push_back(bone.name);
    }
    return toJavaStringArray(env, names);
}

// [x, y, skX, skY, scX, scY, length, parentIndex]
extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_example_rebuilt_MainActivity_getBoneInfo(JNIEnv* env, jobject /*thiz*/,
                                                  jint armatureIndex, jstring boneName) {
    const chroma::ArmatureData* armature = armatureAt(armatureIndex);
    if (armature == nullptr) {
        return nullptr;
    }
    const std::string name = fromJavaString(env, boneName);
    const chroma::BoneData* bone = armature->findBone(name);
    if (bone == nullptr) {
        return nullptr;
    }

    const jfloat values[8] = {
        bone->transform.x,
        bone->transform.y,
        bone->transform.skX,
        bone->transform.skY,
        bone->transform.scX,
        bone->transform.scY,
        bone->length,
        static_cast<jfloat>(bone->parentIndex),
    };
    jfloatArray result = env->NewFloatArray(8);
    if (result != nullptr) {
        env->SetFloatArrayRegion(result, 0, 8, values);
    }
    return result;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_example_rebuilt_MainActivity_getAnimationNames(JNIEnv* env, jobject /*thiz*/,
                                                        jint armatureIndex) {
    const chroma::ArmatureData* armature = armatureAt(armatureIndex);
    if (armature == nullptr) {
        return toJavaStringArray(env, {});
    }
    std::vector<std::string> names;
    names.reserve(armature->animations.size());
    for (const chroma::AnimationData& animation : armature->animations) {
        names.push_back(animation.name);
    }
    return toJavaStringArray(env, names);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_example_rebuilt_MainActivity_getSlotNames(JNIEnv* env, jobject /*thiz*/,
                                                   jint armatureIndex) {
    const chroma::ArmatureData* armature = armatureAt(armatureIndex);
    if (armature == nullptr) {
        return toJavaStringArray(env, {});
    }
    std::vector<std::string> names;
    names.reserve(armature->slots.size());
    for (const chroma::SlotData& slot : armature->slots) {
        names.push_back(slot.name);
    }
    return toJavaStringArray(env, names);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_rebuilt_MainActivity_getAtlasImagePath(JNIEnv* env, jobject /*thiz*/) {
    return env->NewStringUTF(gEngine.atlas().imagePath.c_str());
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_example_rebuilt_MainActivity_getAtlasSize(JNIEnv* env, jobject /*thiz*/) {
    const jint values[2] = {static_cast<jint>(gEngine.atlas().width),
                            static_cast<jint>(gEngine.atlas().height)};
    jintArray result = env->NewIntArray(2);
    if (result != nullptr) {
        env->SetIntArrayRegion(result, 0, 2, values);
    }
    return result;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_rebuilt_MainActivity_getAtlasRegionCount(JNIEnv* /*env*/, jobject /*thiz*/) {
    return static_cast<jint>(gEngine.atlas().regions.size());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_rebuilt_MainActivity_getAtlasRegionName(JNIEnv* env, jobject /*thiz*/,
                                                         jint index) {
    if (index < 0 || static_cast<std::size_t>(index) >= gEngine.atlas().regions.size()) {
        return env->NewStringUTF("");
    }
    return env->NewStringUTF(
        gEngine.atlas().regions[static_cast<std::size_t>(index)].name.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_rebuilt_MainActivity_setBoneOffset(JNIEnv* env, jobject /*thiz*/,
                                                    jstring boneName, jfloat x, jfloat y) {
    gRenderer.setBoneOffset(fromJavaString(env, boneName), x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_rebuilt_MainActivity_clearBoneOffsets(JNIEnv* /*env*/, jobject /*thiz*/) {
    gRenderer.clearBoneOffsets();
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_example_rebuilt_MainActivity_getRenderSpriteData(JNIEnv* env, jobject /*thiz*/,
                                                          jint armatureIndex) {
    std::vector<chroma::SpriteCommand> sprites;
    if (!gRenderer.buildFrame(gEngine.skeleton(), gEngine.atlas(), armatureIndex, sprites)) {
        return toJavaFloatArray(env, {});
    }

    constexpr std::size_t kFloatsPerSprite = 15;
    std::vector<float> values;
    values.reserve(sprites.size() * kFloatsPerSprite);
    for (const chroma::SpriteCommand& sprite : sprites) {
        values.push_back(static_cast<float>(sprite.atlasX));
        values.push_back(static_cast<float>(sprite.atlasY));
        values.push_back(static_cast<float>(sprite.atlasWidth));
        values.push_back(static_cast<float>(sprite.atlasHeight));
        values.push_back(sprite.x);
        values.push_back(sprite.y);
        values.push_back(sprite.width);
        values.push_back(sprite.height);
        values.push_back(sprite.a);
        values.push_back(sprite.b);
        values.push_back(sprite.c);
        values.push_back(sprite.d);
        values.push_back(sprite.tx);
        values.push_back(sprite.ty);
        values.push_back(sprite.alpha);
    }
    return toJavaFloatArray(env, values);
}

extern "C" JNIEXPORT jfloatArray JNICALL
Java_com_example_rebuilt_MainActivity_getArmatureBounds(JNIEnv* env, jobject /*thiz*/,
                                                        jint armatureIndex) {
    const chroma::ArmatureData* armature = armatureAt(armatureIndex);
    if (armature == nullptr) {
        return toJavaFloatArray(env, {});
    }
    const chroma::Aabb& aabb = armature->aabb;
    return toJavaFloatArray(env, {aabb.x, aabb.y, aabb.width, aabb.height});
}
