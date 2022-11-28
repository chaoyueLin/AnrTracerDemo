//
// Created by Charles on 2022/11/19.
//

#ifndef ANRTRACERDEMO_SCOPED_UTF_CHARS_H
#define ANRTRACERDEMO_SCOPED_UTF_CHARS_H

#include <string.h>

#include <jni.h>
#include "nativehelper_utils.h"


class ScopedUtfChars {
public:
    ScopedUtfChars(JNIEnv *env, jstring s) : env_(env), string_(s) {
        if (s == nullptr) {
            utf_chars_ = nullptr;
            jniThrowNullPointerException(env, nullptr);
        } else {
            utf_chars_ = env->GetStringUTFChars(s, nullptr);
        }
    }

    ScopedUtfChars(ScopedUtfChars &&rhs) :
            env_(rhs.env_), string_(rhs.string_), utf_chars_(rhs.utf_chars_) {
        rhs.env_ = nullptr;
        rhs.string_ = nullptr;
        rhs.utf_chars_ = nullptr;
    }

    ~ScopedUtfChars() {
        if (utf_chars_) {
            env_->ReleaseStringUTFChars(string_, utf_chars_);
        }
    }

    ScopedUtfChars &operator=(ScopedUtfChars &&rhs) {
        if (this != &rhs) {
            // Delete the currently owned UTF chars.
            this->~ScopedUtfChars();

            // Move the rhs ScopedUtfChars and zero it out.
            env_ = rhs.env_;
            string_ = rhs.string_;
            utf_chars_ = rhs.utf_chars_;
            rhs.env_ = nullptr;
            rhs.string_ = nullptr;
            rhs.utf_chars_ = nullptr;
        }
        return *this;
    }

    const char *c_str() const {
        return utf_chars_;
    }

    size_t size() const {
        return strlen(utf_chars_);
    }

    const char &operator[](size_t n) const {
        return utf_chars_[n];
    }

private:
    JNIEnv *env_;
    jstring string_;
    const char *utf_chars_;

    ScopedUtfChars(const ScopedUtfChars &) = delete;

    void operator=(const ScopedUtfChars &) = delete;
};

#endif //ANRTRACERDEMO_SCOPED_UTF_CHARS_H
