#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_rebuilt_MainActivity_getEngineVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("ChromaToons Native Engine 1.0.0 (C++20 / JNI)");
}
