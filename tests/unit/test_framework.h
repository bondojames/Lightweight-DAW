// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <vector>

namespace ldaw::test {

struct Case { const char* name; void (*fn)(); };
std::vector<Case>& registry();
void fail(const char* file, int line, const char* expression);
struct Registrar { Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); } };

} // namespace ldaw::test

#define TEST_CASE(id) \
    static void id(); \
    static const ::ldaw::test::Registrar registrar_##id(#id, id); \
    static void id()

#define CHECK(expr) do { if (!(expr)) ::ldaw::test::fail(__FILE__, __LINE__, #expr); } while (false)
