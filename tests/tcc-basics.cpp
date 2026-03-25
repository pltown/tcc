#include "tcc-harness.h"
#include "common.h"

#include <iostream>
using namespace std;

// SCENARIO_TEMPLATE(scenario name, type, list of types)
// SCENARIO_TEMPLATE_DEFINE(scenario name, type, list of types)

static auto const *cGCallsF = R"c(
    void f(int i, int j) { int c = i + j; }
    void g(int i) { f(i, i + 1); }
)c";

static auto const *cGMultiCallsF = R"c(
    void f();
    void g() { f(); f(); }
)c";

static auto const *cGlobalVar = R"c(
    int gi = 0;
    void h(int p) { gi = p; }
)c";

static auto const *cCallCycle = R"c(
    void h(void);
    void g(void);
    void f(void) { h(); }
    void h(void) { g(); }
    void g(void) { f(); }
)c";

static auto const *cComplete = R"c(
    void f(int i, int j) { int c = i + j; }
    int main() {
        i = 0; j = 1;
        f(i, j);
        return 0;
    }
)c";

SCENARIO("TCC key construction") {
    GIVEN("A C code with functions and assignments") {
        // At least one non-main function, one global variable
        auto const *code = R"c(
            int gi;
            int ngi;
            void f(int i, int j) { int c = i; c = j; }

            void hof(int i, void(*fn)(int, int)) {
                fn(i, i);
            }

            int main() {
                 int i = gi;
                 f(i, i);
                 hof(i, f);
                 return 0;
            }
        )c";

        WHEN("Aliasing or aliased declarations are visited") {
            auto census = analyze(code);
            TestDB results = std::move(census);

            THEN("All parameter keys should be in SSA form") {
                CHECK_FALSE(results.hasNode("i"));
                CHECK_FALSE(results.hasNode("f.i"));
                CHECK_FALSE(results.hasNode("f.$i"));
                CHECK(results.hasNode("f.$0"));
                CHECK(results.hasNode("f.$1"));
                CHECK_FALSE(results.hasNode("f.$2"));
            }

            THEN("All relevant local variable key should be in SSA form") {
                CHECK(results.hasNode("f.c"));
            }

            THEN("All relevant global variable key should be in SSA-like form") {
                CHECK(results.hasNode("::.gi"));
            }

            THEN("All relevant fptr param keys should be in SSA form") {
                CHECK(results.hasNode("hof.$0"));
                CHECK(results.hasNode("hof.$1"));
                CHECK(results.hasNode("hof.$1.$0"));
                CHECK(results.hasNode("hof.$1.$1"));
                CHECK_FALSE(results.hasNode("hof.$1.$2"));
                CHECK_FALSE(results.hasNode("hof.$1.i"));
                CHECK_FALSE(results.hasNode("hof.$0.$1"));
                CHECK_FALSE(results.hasNode("hof.$0.i"));
            }
        }
    }

    GIVEN("C code with unary and binary operations") {
        auto const *code = R"c(
        int main() {
            int i = 1;
            int *pi = &i;
            int j = *pi;
            int c = i + 1;
            int d = i + c;
            return 0;
        }
        )c";

        WHEN("Aliasing/aliased expressions are visited") {
            auto census = analyze(code);
            TestDB results = std::move(census);

            fmt::print(stdout, "TCCNodes:\n");
            for(auto const &[k, v]: results.nodes_.db()) {
                fmt::print(stdout, "{} = {{{}}}\n", k, String(v));
            }

            THEN("Unary expressions are recorded in SSA form") {
                CHECK(results.hasNode(".__&i__.main.i"));
                CHECK(results.hasNode(".__*pi__.main.pi"));
            }

            THEN("Binary expressions are recorded in SSA form") {
                // TODO: fix binary operations
                // May not be in this form but current form could be the cause for errors
                CHECK(results.hasNode(".__i + 1__.main.i"));
                CHECK(results.hasNode(".__i + c__.main.i"));
            }
        }

    }
}

SCENARIO("Fptr resolution") {
    GIVEN("C higher-order function") {
        auto const *code = R"c(
            void f(int *pi) {
                char c = *(char*)pi;
            }
            void g(int *pi) {
                char c = *(char*)pi;
            }
            void hof(int *pi, void (*fn)(int*)) {
                fn(pi);
            }
            int main() {
                int i = 1;
                hof(&i, f);
                int j = 2;
                hof(&j, g);
                return 0;
            }
        )c";

        WHEN("History is built for HoF call") {
            auto census = analyze(code);
            TestDB results = std::move(census);

            THEN("HoF arg history should be extended with the correct function") {
                auto children = [](auto const &key) -> auto {
                    // append to children till H(key).nexts = 0
                };
                auto contains = [](auto const &key) -> auto {
                    std::set<std::string> children;
                    // capture history of key
                    // return a function that checks key's history's children for an input child key
                };

                // Assert f.$0 is in H(main.i)
                // Assert g.$0 is not in H(main.i)
                // Assert f.$0 is not in H(main.j)
                // Assert g.$0 is in H(main.j)
            }
        }
    }
}

/*
SCENARIO("Aliasing") {
    GIVEN("A C code with some functions") {
        WHEN("a is assigned to b") {
            THEN("history of a is extended by history of b") {
                CHECK(constraint.dom() == "a"
                        && constriant.child() == "b");
            }
        }

        WHEN("f(p1, p2) is called with (a, b)") {
            THEN("history of p1 extends history of a, and so on") {
                CHECK(constraints[0].dom() == "a"
                        && constraints[0].child() == "p1");
                CHECK(constraints[1].dom() == "b"
                        && constraints[1].child() == "p2");
            }
        }
    }
}
*/
