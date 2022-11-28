
#include "tracer.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <cstdio>
#include <ctime>
#include <csignal>
#include <thread>
#include <memory>
#include <string>
#include <optional>
#include <sstream>
#include <fstream>

#include "nativehelper/managed_jnienv.h"
#include "nativehelper/scoped_utf_chars.h"
#include "AnrDumper.h"
#include "Support.h"
#include "SignalHandler.h"
#include "bytehook.h"

#define PROP_VALUE_MAX  92
#define PROP_SDK_NAME "ro.build.version.sdk"
#define HOOK_CONNECT_PATH "/dev/socket/tombstoned_java_trace"
#define HOOK_OPEN_PATH "/data/anr/traces.txt"


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
    jmethodID AnrDetector_onANRDumped;
    jmethodID AnrDetector_onANRDumpTrace;
    jmethodID AnrDetector_onPrintTrace;

    jmethodID AnrDetector_onNativeBacktraceDumped;

} gJ;

bool anrDumpCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) {
        return false;
    }
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumped);
    return true;
}

bool anrDumpTraceCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onANRDumpTrace);
    return true;
}

bool nativeBacktraceDumpCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onNativeBacktraceDumped);
    return true;
}

bool printTraceCallback() {
    JNIEnv *env = JniInvocation::getEnv();
    if (!env) return false;
    env->CallStaticVoidMethod(gJ.AnrDetective, gJ.AnrDetector_onPrintTrace);
    return true;
}

int (*original_connect)(int __fd, const struct sockaddr *__addr, socklen_t __addr_length);

int my_connect(int __fd, const struct sockaddr *__addr, socklen_t __addr_length) {
    ALOGI("my_connect hook");
    BYTEHOOK_STACK_SCOPE();
    if (__addr != nullptr) {
        if (strcmp(__addr->sa_data, HOOK_CONNECT_PATH) == 0) {
            signalCatcherTid = gettid();
            isTraceWrite = true;
        }
    }
    int r = BYTEHOOK_CALL_PREV(my_connect, __fd, __addr, __addr_length);
    return r;
}

int (*original_open)(const char *pathname, int flags, mode_t mode);

static void hooked_callback(bytehook_stub_t task_stub, int status_code, const char *caller_path_name,
                            const char *sym_name, void *new_fun, void *pre_func, void *arg) {
    if (BYTEHOOK_STATUS_CODE_ORIG_ADDR) {
        ALOGV("hook_tag >>>>> save original adderss");
    }
};

int my_open(const char *pathname, int flags, mode_t mode) {
    ALOGI("my_open hook");
    BYTEHOOK_STACK_SCOPE();
    if (pathname != nullptr) {
        if (strcmp(pathname, HOOK_CONNECT_PATH) == 0) {
            signalCatcherTid = gettid();
            isTraceWrite = true;
        }
    }
    int r = BYTEHOOK_CALL_PREV(my_open, pathname, flags, mode);
    return r;
}

void writeAnr(const std::string &content, const std::string &filePath) {
    unHookAnrTraceWrite();
    std::string to;
    std::ofstream outfile;
    outfile.open(filePath);
    outfile << content;
}

ssize_t (*original_write)(int fd, const void *const __pass_object_size0 buf, size_t count);

ssize_t my_write(int fd, const void *const buf, size_t count) {
    BYTEHOOK_STACK_SCOPE();
    if (isTraceWrite && gettid() == signalCatcherTid) {
        isTraceWrite = false;
        signalCatcherTid = 0;
        if (buf != nullptr) {
            std::string targetFilePath;
            if (fromMyPrintTrace) {
                targetFilePath = printTracePathString;
            } else {
                targetFilePath = anrTracePathString;
            }
            if (!targetFilePath.empty()) {
                char *content = (char *) buf;
                writeAnr(content, targetFilePath);
                if (!fromMyPrintTrace) {
                    anrDumpTraceCallback();
                } else {
                    printTraceCallback();
                }
                fromMyPrintTrace = false;
            }
        }
    }
    return BYTEHOOK_CALL_PREV(my_write, fd, buf, count);
}

int getApiLevel() {
    char buf[PROP_VALUE_MAX];
    int len = __system_property_get(PROP_SDK_NAME, buf);
    if (len <= 0)
        return 0;

    return atoi(buf);
}


void hookAnrTraceWrite(bool isSigUser) {
    ALOGI("hookAnrTraceWrite isSigUser=%d", isSigUser);
    int apiLevel = getApiLevel();
    ALOGI("hookAnrTraceWrite apiLevel=%d", apiLevel);
    if (apiLevel < 19) {
        return;
    }

    if (!fromMyPrintTrace && isSigUser) {
        return;
    }

    if (isHooking) {
        return;
    }

    isHooking = true;
    if (apiLevel >= 27) {
        ALOGI("hook connect");
        bytehook_hook_single("libcutils.so", nullptr, "connect", (void *) my_connect, hooked_callback, nullptr);
    } else {
        ALOGI("hook open");
        bytehook_hook_single("libart.so", nullptr, "open", (void *) my_open, hooked_callback, nullptr);
    }

    if (apiLevel >= 30 || apiLevel == 25 || apiLevel == 24) {
        bytehook_hook_single("libc.so", nullptr,
                             "write", (void *) my_write, hooked_callback, nullptr);
    } else if (apiLevel == 29) {
        bytehook_hook_single("libbase.so", nullptr,
                             "write", (void *) my_write, hooked_callback, nullptr);
    } else {
        bytehook_hook_single("libart.so", nullptr,
                             "write", (void *) my_write, hooked_callback, nullptr);
    }
}

void unHookAnrTraceWrite() {
    isHooking = false;
}

static void nativeInitSignalAnrDetective(JNIEnv *env, jclass, jstring anrTracePath, jstring printTracePath) {
    const char *anrTracePathChar = env->GetStringUTFChars(anrTracePath, nullptr);
    const char *printTracePathChar = env->GetStringUTFChars(printTracePath, nullptr);
    anrTracePathString = std::string(anrTracePathChar);
    printTracePathString = std::string(printTracePathChar);
    ALOGV("anrTracePath=%s", anrTracePathChar);
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
