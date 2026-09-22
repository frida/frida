#include <jni.h>

#include "com_mesonbuild_JniTest.h"

JNIEXPORT jint JNICALL Java_com_mesonbuild_JniTest_jni_1test
  (JNIEnv *env, jclass clazz)
{
    return (jint)0xdeadbeef;
}
