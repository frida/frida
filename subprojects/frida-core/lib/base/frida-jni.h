#ifndef __FRIDA_JNI_H__
#define __FRIDA_JNI_H__

#include <jni.h>

G_BEGIN_DECLS

typedef jint         (* JNIDestroyJavaVMFunc)         (JavaVM *);
typedef jint         (* JNIAttachCurrentThreadFunc)   (JavaVM *, JNIEnv **, void *);
typedef jint         (* JNIDetachCurrentThreadFunc)   (JavaVM *);
typedef jint         (* JNIGetEnvFunc)                (JavaVM *, JNIEnv **, jint);

typedef jclass       (* JNIFindClassFunc)             (JNIEnv *, const char *);

typedef jthrowable   (* JNIExceptionOccurredFunc)     (JNIEnv *);
typedef void         (* JNIExceptionDescribeFunc)     (JNIEnv *);
typedef void         (* JNIExceptionClearFunc)        (JNIEnv *);

typedef jint         (* JNIPushLocalFrameFunc)        (JNIEnv *, jint);
typedef jobject      (* JNIPopLocalFrameFunc)         (JNIEnv *, jobject);

typedef jobject      (* JNINewGlobalRefFunc)          (JNIEnv *, jobject);
typedef void         (* JNIDeleteGlobalRefFunc)       (JNIEnv *, jobject);
typedef void         (* JNIDeleteLocalRefFunc)        (JNIEnv *, jobject);

typedef jobject      (* JNINewObjectFunc)             (JNIEnv *, jclass, jmethodID, ...);

typedef jclass       (* JNIGetObjectClassFunc)        (JNIEnv *, jobject);
typedef jmethodID    (* JNIGetMethodIDFunc)           (JNIEnv *, jclass, const char *, const char *);

typedef jobject      (* JNICallObjectMethodFunc)      (JNIEnv *, jobject, jmethodID, ...);

typedef jstring      (* JNINewStringUTFFunc)          (JNIEnv *, const char *);
typedef const char * (* JNIGetStringUTFCharsFunc)     (JNIEnv *, jstring, jboolean *);
typedef void         (* JNIReleaseStringUTFCharsFunc) (JNIEnv *, jstring, const char *);

typedef jboolean     (* JNIExceptionCheckFunc)        (JNIEnv *);

G_END_DECLS

#endif
