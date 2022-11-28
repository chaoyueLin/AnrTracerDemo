//
// Created by Charles on 2022/11/19.
//

#ifndef ANRTRACERDEMO_NATIVEHELPER_UTILS_H
#define ANRTRACERDEMO_NATIVEHELPER_UTILS_H

#include <jni.h>

#ifndef NATIVEHELPER_JNIHELP_H_

// This seems a header-only include. Provide NPE throwing.
static inline int jniThrowNullPointerException(JNIEnv *env, const char *msg) {
    if (env->ExceptionCheck()) {
        // Drop any pending exception.
        env->ExceptionClear();
    }

    jclass e_class = env->FindClass("java/lang/NullPointerException");
    if (e_class == nullptr) {
        return -1;
    }

    if (env->ThrowNew(e_class, msg) != JNI_OK) {
        env->DeleteLocalRef(e_class);
        return -1;
    }

    env->DeleteLocalRef(e_class);
    return 0;
}

#endif  // NATIVEHELPER_JNIHELP_H_

#endif //ANRTRACERDEMO_NATIVEHELPER_UTILS_H
