

#ifndef ANRTRACERDEMO_ANRDUMPER_H_
#define ANRTRACERDEMO_ANRDUMPER_H_

#include "SignalHandler.h"
#include <functional>
#include <string>
#include <optional>
#include <jni.h>

namespace Tracer {

    class AnrDumper : public SignalHandler {
    public:
        AnrDumper(const char *anrTraceFile, const char *printTraceFile);

        virtual ~AnrDumper();

    private:
        void handleSignal(int sig, const siginfo_t *info, void *uc) final;

        void handleDebuggerSignal(int sig, const siginfo_t *info, void *uc) final;

        static void *nativeBacktraceCallback(void *arg);
    };
}   // namespace Tracer

#endif  // ANRTRACERDEMO_ANRDUMPER_H_