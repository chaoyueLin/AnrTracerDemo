

#ifndef ANRTRACERDEMO_LOGGING_H_
#define ANRTRACERDEMO_LOGGING_H_

#ifdef __ANDROID__

#include <android/log.h>

#endif

#ifdef ALOGV
#undef ALOGV
#endif

namespace Tracer {
    namespace Logging {

        static constexpr const char *kLogTag = "Tracer";

#if defined(__GNUC__)

        __attribute__((__format__(printf, 1, 2)))
#endif
        static inline void ALOGV(const char *fmt, ...) {
#ifdef __ANDROID__
            va_list ap;
            va_start(ap, fmt);
            __android_log_vprint(ANDROID_LOG_VERBOSE, kLogTag, fmt, ap);
            va_end(ap);
#endif
        }

#if defined(__GNUC__)

        __attribute__((__format__(printf, 1, 2)))
#endif
        static inline void ALOGI(const char *fmt, ...) {
#ifdef __ANDROID__
            va_list ap;
            va_start(ap, fmt);
            __android_log_vprint(ANDROID_LOG_INFO, kLogTag, fmt, ap);
            va_end(ap);
#endif
        }

#if defined(__GNUC__)

        __attribute__((__format__(printf, 1, 2)))
#endif
        static inline void ALOGE(const char *fmt, ...) {
#ifdef __ANDROID__
            va_list ap;
            va_start(ap, fmt);
            __android_log_vprint(ANDROID_LOG_ERROR, kLogTag, fmt, ap);
            va_end(ap);
#endif
        }

    }   // namespace Logging
}   // namespace Tracer

using ::Tracer::Logging::ALOGV;
using ::Tracer::Logging::ALOGI;
using ::Tracer::Logging::ALOGE;

#endif  // ANRTRACERDEMO_LOGGING_H_
