

#include "SignalHandler.h"

#include <malloc.h>
#include <syscall.h>
#include <dirent.h>
#include <unistd.h>

#include <mutex>
#include <vector>
#include <algorithm>
#include <cinttypes>

#include "Logging.h"
#include "Support.h"


namespace Tracer {

    struct sigaction sOldHandlers;
    struct sigaction sNativeBacktraceOldHandlers;
    static bool sHandlerInstalled = false;
    static bool sNativeBacktraceHandlerInstalled = false;

    static std::vector<SignalHandler *> *sHandlerStack = nullptr;
    static std::mutex sHandlerStackMutex;
    static std::mutex sNativeBacktraceHandlerStackMutex;
    static bool sStackInstalled = false;
    static stack_t sOldStack;
    static stack_t sNewStack;

    static void installAlternateStackLocked() {
        if (sStackInstalled)
            return;

        memset(&sOldStack, 0, sizeof(sOldStack));
        memset(&sNewStack, 0, sizeof(sNewStack));
        static constexpr unsigned kSigStackSize = std::max(16384, SIGSTKSZ);

        if (sigaltstack(nullptr, &sOldStack) == -1 || !sOldStack.ss_sp || sOldStack.ss_size < kSigStackSize) {
            sNewStack.ss_sp = calloc(1, kSigStackSize);
            sNewStack.ss_size = kSigStackSize;
            if (sigaltstack(&sNewStack, nullptr) == -1) {
                free(sNewStack.ss_sp);
                return;
            }
        }

        sStackInstalled = true;
    }

    bool SignalHandler::installHandlersLocked() {
        if (sHandlerInstalled) {
            return false;
        }

        if (sigaction(TARGET_SIG, nullptr, &sOldHandlers) == -1) {
            return false;
        }

        struct sigaction sa{};
        sa.sa_sigaction = signalHandler;
        sa.sa_flags = SA_ONSTACK | SA_SIGINFO | SA_RESTART;

        if (sigaction(TARGET_SIG, &sa, nullptr) == -1) {
            return false;
        }

        sHandlerInstalled = true;
        return true;
    }

    bool SignalHandler::installNativeBacktraceHandlersLocked() {
        if (sNativeBacktraceHandlerInstalled) {
            return false;
        }

        if (sigaction(BIONIC_SIGNAL_DEBUGGER, nullptr, &sNativeBacktraceOldHandlers) == -1) {
            return false;
        }

        struct sigaction sa{};
        sa.sa_sigaction = debuggerSignalHandler;
        sa.sa_flags = SA_ONSTACK | SA_SIGINFO | SA_RESTART;

        if (sigaction(BIONIC_SIGNAL_DEBUGGER, &sa, nullptr) == -1) {
            return false;
        }

        sNativeBacktraceHandlerInstalled = true;
        return true;
    }

    void SignalHandler::installDefaultHandler(int sig) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sigemptyset(&sa.sa_mask);
        sa.sa_handler = SIG_DFL;
        sa.sa_flags = SA_RESTART;
        sigaction(sig, &sa, nullptr);
    }

    void SignalHandler::restoreHandlersLocked() {
        if (!sHandlerInstalled)
            return;

        if (sigaction(TARGET_SIG, &sOldHandlers, nullptr) == -1) {
            installDefaultHandler(TARGET_SIG);
        }

        sHandlerInstalled = false;
    }

    void SignalHandler::restoreNativeBacktraceHandlersLocked() {
        if (!sNativeBacktraceHandlerInstalled)
            return;

        if (sigaction(BIONIC_SIGNAL_DEBUGGER, &sNativeBacktraceOldHandlers, nullptr) == -1) {
            installDefaultHandler(BIONIC_SIGNAL_DEBUGGER);
        }

        sNativeBacktraceHandlerInstalled = false;
    }

    static void restoreAlternateStackLocked() {
        if (!sStackInstalled)
            return;

        stack_t current_stack;
        if (sigaltstack(nullptr, &current_stack) == -1)
            return;

        if (current_stack.ss_sp == sNewStack.ss_sp) {
            if (sOldStack.ss_sp) {
                if (sigaltstack(&sOldStack, nullptr) == -1)
                    return;
            } else {
                stack_t disable_stack;
                disable_stack.ss_flags = SS_DISABLE;
                if (sigaltstack(&disable_stack, nullptr) == -1)
                    return;
            }
        }

        free(sNewStack.ss_sp);
        sStackInstalled = false;
    }

    void SignalHandler::signalHandler(int sig, siginfo_t *info, void *uc) {
        std::unique_lock<std::mutex> lock(sHandlerStackMutex);

        for (auto it = sHandlerStack->rbegin(); it != sHandlerStack->rend(); ++it) {
            (*it)->handleSignal(sig, info, uc);
        }

        lock.unlock();
    }

    void SignalHandler::debuggerSignalHandler(int sig, siginfo_t *info, void *uc) {
        std::unique_lock<std::mutex> lock(sNativeBacktraceHandlerStackMutex);

        for (auto it = sHandlerStack->rbegin(); it != sHandlerStack->rend(); ++it) {
            (*it)->handleDebuggerSignal(sig, info, uc);
        }

        lock.unlock();
    }

    SignalHandler::SignalHandler() {
        std::lock_guard<std::mutex> lock(sHandlerStackMutex);

        if (!sHandlerStack)
            sHandlerStack = new std::vector<SignalHandler *>;

        installAlternateStackLocked();
        installHandlersLocked();
        installNativeBacktraceHandlersLocked();
        sHandlerStack->push_back(this);
    }

    SignalHandler::~SignalHandler() {
        std::lock_guard<std::mutex> lock(sHandlerStackMutex);

        auto it = std::find(sHandlerStack->begin(), sHandlerStack->end(), this);
        sHandlerStack->erase(it);
        if (sHandlerStack->empty()) {
            delete sHandlerStack;
            sHandlerStack = nullptr;
            restoreAlternateStackLocked();
            restoreNativeBacktraceHandlersLocked();
            restoreHandlersLocked();
        }
    }

}   // namespace Tracer
