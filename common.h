//
// Created by SonJihoon on 2016. 1. 1..
//

#ifndef KALEIDOSCOPE_COMMON_H
#define KALEIDOSCOPE_COMMON_H

#include <memory>
#include "Ast.h"

std::unique_ptr<ExprAst> Error(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}

std::unique_ptr<PrototypeAst> ErrorP(const char *Str) {
    return Error(Str);
}

namespace helper {
    // Cloning make_unique here until it's standard in C++14.
    // Using a namespace to avoid conflicting with MSVC's std::make_unique (which
    // ADL can sometimes find in unqualified calls).
    template <class T, class... Args>
    static typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type make_unique(Args &&... args) {
        return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    };
}

#endif //KALEIDOSCOPE_COMMON_H
