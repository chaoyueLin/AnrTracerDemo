
#include "tracer.h"

#include <jni.h>
#include <string>
#include <unistd.h>
#include "nativehelper/managed_jnienv.h"
#include "nativehelper/scoped_utf_chars.h"
#include "AnrDumper.h"
#include "Support.h"
#include "SignalHandler.h"

using namespace Tracer;
using namespace Logging;

static std::optional<AnrDumper> sAnrDumper;
static bool isTraceWrite = false;
static bool fromMyPrintTrace = false;
static bool isHooking = false;
static std::string anrTracePathString;
static std::string printTracePathString;
static int signalCatcherTid;
static struct StacktraceJNI {
    jclass AnrDetective;
    jclass ThreadPriorityDetective;
    jclass TouchEventLagTracer;
    jmethodID AnrDetector_onANRDumped;
    jmethodID AnrDetector_onANRDumpTrace;
    jmethodID AnrDetector_onPrintTrace;

    jmethodID AnrDetector_onNativeBacktraceDumped;

} gJ;

bool anrDumpCallback() {}

bool anrDumpTraceCallback() {}

bool nativeBacktraceDumpCallback() {}

bool printTraceCallback() {}

void hookAnrTraceWrite(bool isSigUser) {}

void unHookAnrTraceWrite() {
    isHooking = false;
}

static void nativeInitSignalAnrDetective(JNIEnv *env, jclass, jstring anrTracePath, jstring printTracePath) {
    const char *anrTracePathChar = env->GetStringUTFChars(anrTracePath, nullptr);
    const char *printTracePathChar = env->GetStringUTFChars(printTracePath, nullptr);
    anrTracePathString = std::string(anrTracePathChar);
    printTracePathString = std::string(printTracePathChar);
    ALOGV("anrTracePath=%s",anrTracePathChar);
    sAnrDumper.emplace(anrTracePathChar, printTracePathChar);
}

static void nativeFreeSignalAnrDetective(JNIEnv *env, jclass) {
    sAnrDumper.reset();
}

static void nativePrintTrace() {
    fromMyPrintTrace = true;
    kill(getpid(), SIGQUIT);
}

template<typename T, std::size_t sz>
static inline constexpr std::size_t NELEM(const T(&)[sz]) { return sz; }

static const JNINativeMethod ANR_METHODS[] = {
        {"nativeInitSignalAnrDetective", "(Ljava/lang/String;Ljava/lang/String;)V", (void *) nativeInitSignalAnrDetective},
        {"nativeFreeSignalAnrDetective", "()V",                                     (void *) nativeFreeSignalAnrDetective},
        {"nativePrintTrace",             "()V",                                     (void *) nativePrintTrace},
};


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    JniInvocation::init(vm);

    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
        return -1;

    jclass anrDetectiveCls = env->FindClass("com/example/tracer/NativeLib");
    if (!anrDetectiveCls)
        return -1;
    gJ.AnrDetective = static_cast<jclass>(env->NewGlobalRef(anrDetectiveCls));
    gJ.AnrDetector_onANRDumped =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumped", "()V");
    gJ.AnrDetector_onANRDumpTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onANRDumpTrace", "()V");
    gJ.AnrDetector_onPrintTrace =
            env->GetStaticMethodID(anrDetectiveCls, "onPrintTrace", "()V");

    gJ.AnrDetector_onNativeBacktraceDumped =
            env->GetStaticMethodID(anrDetectiveCls, "onNativeBacktraceDumped", "()V");


    if (env->RegisterNatives(
            anrDetectiveCls, ANR_METHODS, static_cast<jint>(NELEM(ANR_METHODS))) != 0)
        return -1;

    env->DeleteLocalRef(anrDetectiveCls);

    return JNI_VERSION_1_6;
}
