//
// Created by Charles on 2022/11/19.
//

#ifndef ANRTRACERDEMO_MANAGED_JNIENV_H
#define ANRTRACERDEMO_MANAGED_JNIENV_H


#include <jni.h>

namespace JniInvocation {

    void init(JavaVM *vm);

    JavaVM *getJavaVM();

    JNIEnv *getEnv();

}


#endif //ANRTRACERDEMO_MANAGED_JNIENV_H
