//
// Created by Charles on 2022/11/19.
//

#ifndef ANRTRACERDEMO_TRACER_H
#define ANRTRACERDEMO_TRACER_H

bool anrDumpCallback();

bool anrDumpTraceCallback();

bool nativeBacktraceDumpCallback();

bool printTraceCallback();

void hookAnrTraceWrite(bool isSigUser);

void unHookAnrTraceWrite();

#endif //ANRTRACERDEMO_TRACER_H
