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

constexpr auto errShouldExtend(std::string const &a, std::string const &b) {
    return ">> History of " + a + " should be extended by " + b;
};

constexpr auto errShouldNotExtend(std::string const &a, std::string const &b) {
    return ">> History of " + a + " should not be extended by " + b;
};

auto hasContext(CastHistoryInstance const &chi, std::string_view cid) -> bool {
    for(auto const &c: chi.context()) {
        if(c.id() == cid) {
            return true;
        }
    }
    return false;
}

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

        INFO("Code under test:\n", std::string(code));

        WHEN("Aliasing or aliased declarations are visited") {
            auto census = analyze(code);
            TestDB results = std::move(census);
            CAPTURE_TCCNODES(results.nodes_);

            THEN("All parameter keys should be in SSA form") {
                REQUIRE(results.hasNode("f.$0"));
                REQUIRE(results.hasNode("f.$1"));

                CHECK_FALSE(results.hasNode("i"));
                CHECK_FALSE(results.hasNode("f.i"));
                CHECK_FALSE(results.hasNode("f.$i"));
                CHECK_FALSE(results.hasNode("f.$2"));
            }

            THEN("All relevant local variable key should be in SSA form") {
                REQUIRE(results.hasNode("f.c"));
            }

            THEN("All relevant global variable key should be in SSA-like form") {
                REQUIRE(results.hasNode("::.gi"));
            }

            THEN("All relevant fptr param keys should be in SSA form") {
                REQUIRE(results.hasNode("hof.$0"));
                REQUIRE(results.hasNode("hof.$1"));
                REQUIRE(results.hasNode("hof.$1.$0"));
                REQUIRE(results.hasNode("hof.$1.$1"));
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

        INFO("Code under test:\n", std::string(code));

        WHEN("Aliasing/aliased expressions are visited") {
            auto census = analyze(code);
            TestDB results = std::move(census);
            CAPTURE_TCCNODES(results.nodes_);

            THEN("Unary expressions are recorded in SSA form") {
                REQUIRE(results.hasNode(".__&i__.main.i"));
                REQUIRE(results.hasNode(".__*pi__.main.pi"));
            }

            THEN("Binary expressions are recorded in SSA form") {
                // TODO: fix binary operations
                // May not be in this form but current form could be the cause for errors
                REQUIRE(results.hasNode(".__i + 1__.main.i"));
                REQUIRE(results.hasNode(".__i + c__.main.i"));
            }
        }

    }
}

SCENARIO("Fptr resolution") {
    GIVEN("Higher-order function calls in same scope") {
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

        INFO("Code under test:\n", std::string(code));

        WHEN("History is built for higher-order function call") {
            auto census = analyze(code);
            TestDB results = std::move(census);
            results.makeInstances();
            //CAPTURE_TCCNODES(results.nodes_);

            THEN("Higer-order arg history should be extended with the correct function") {
                auto iContains = results.instanceFinderFor("main.i");
                auto jContains = results.instanceFinderFor("main.j");

                REQUIRE_MESSAGE(iContains("f.$0").has_value(), errShouldExtend("main.i", "f.$0"));
                CHECK_FALSE_MESSAGE(iContains("g.$0").has_value(), errShouldNotExtend("main.i", "g.$0"));

                REQUIRE_MESSAGE(jContains("g.$0").has_value(), errShouldExtend("main.j", "g.$0"));
                CHECK_FALSE_MESSAGE(jContains("f.$0").has_value(), errShouldNotExtend("main.j", "f.$0"));
            }
        }
    }
}

SCENARIO("Conditional-cast") {
    GIVEN("A condition-based cast") {
        auto const *code = R"c(
            int flag = 0;
            void flaggedOp(void *pv) {
                int *i = 0;
                char *c = 0;
                double *d = 0;
                switch(flag) {
                    case 0: {
                        i = (int*) pv;
                        *i = *i + 1;
                        break;
                    }
                    case 1:
                    case 2: {
                        c = (char*) pv;
                        *c = 'a';
                        //printf("%c", *c);
                        break;
                    }
                    case 3: {
                        d = (double*) pv;
                        *d = *d + 1.0;
                        break;
                    }
                }
            }

            int main() {
                int x = 1;
                flaggedOp(&x);
                return 0;
            }
        )c";

        INFO("Code under test:\n", std::string(code));

        WHEN("cast history is instantiated") {
            auto census = analyze(code);
            TestDB results = std::move(census);
            results.makeInstances();
            CAPTURE_TCCNODES(results.nodes_);

            auto errIncorrectContext = ">> Incorrect condition context";

            THEN("condition for a cast should be part of its history") {
                // Check that history is instantiated for flaggedOp.$0
                REQUIRE(results.hasInstance("flaggedOp.$0"));
                REQUIRE(results.instances_.at("flaggedOp.$0").nexts().empty() == false);

                // Check that history has flaggedOp.$0 => int*
                auto flaggedOp0Child = results.instanceFinderFor("flaggedOp.$0");
                auto c1 = flaggedOp0Child("flaggedOp.i");
                REQUIRE_MESSAGE(c1.has_value(), errShouldExtend("flaggedOp.$0", "flaggedOp.i"));
                //  under condition context flag == 0
                CHECK_MESSAGE(hasContext(*c1, "flag == 0"), errIncorrectContext);
                // and not under condition flag == 1;
                CHECK_FALSE_MESSAGE(hasContext(*c1, "flag == 1"), errIncorrectContext);
                //if(c.kind == tcc::CastContext::SwitchCondition) {

                // Check that history has flaggedOp.$0 => char*
                auto c2 = flaggedOp0Child("flaggedOp.c");
                REQUIRE_MESSAGE(c2.has_value(), errShouldExtend("flaggedOp.$0", "flaggedOp.c"));
                // under condition context flag == 1
                CHECK_MESSAGE(hasContext(*c2, "flag == 1 | 2"), errIncorrectContext);
                // and not under condition flag == 0;
                CHECK_FALSE_MESSAGE(hasContext(*c2, "flag == 0"), errIncorrectContext);

                // Check that history has flaggedOp.$0 => double*
                auto c3 = flaggedOp0Child("flaggedOp.d");
                REQUIRE_MESSAGE(c3.has_value(), errShouldExtend("flaggedOp.$0", "flaggedOp.d"));
                // under condition context flag == 3
                CHECK_MESSAGE(hasContext(*c3, "flag == 3"), errIncorrectContext);
                // and not under other conditions
                CHECK_FALSE_MESSAGE(hasContext(*c3, "flag == 0"), errIncorrectContext);
                CHECK_FALSE_MESSAGE(hasContext(*c3, "flag == 1"), errIncorrectContext);
                CHECK_FALSE_MESSAGE(hasContext(*c3, "flag == 2"), errIncorrectContext);
                CHECK_FALSE_MESSAGE(hasContext(*c3, "flag == 1 | 2"), errIncorrectContext);

                // Check casts with scattered condition (interprocedural cast)
            }
        }
    }
}

SCENARIO("Function-return-alias") {
    GIVEN("An assignment involving a function call") {
        auto const *code = R"c(
            int* f(void *pi) {
                return (int*)pi;
            }

            int* g(int *pa, int *pb) {
                return pa;
            }

            int h(void *pa, int *pb) {
                return *(int*)pa + *pb;
            }

            int* f2(void *pi) {
                return (int*)pi;
            }
            int* hof(int *pa, int*(*pf)(void*)) {
                return pf(pa);
            }

            int main() {
                int i = 0;
                int *j = f(&i);
                int *k = g(&i, j);
                int l = h(&i, j);

                int *m = hof(j, f2);

                return 0;
            }
        )c";

        INFO("Code under test:\n", std::string(code));

        WHEN("cast history is instantiated") {
            auto census = analyze(code);
            TestDB results = std::move(census);
            results.makeInstances();
            //CAPTURE_TCCNODES(results.nodes_);

            THEN("function return should be in history of variable aliasing the return") {
                REQUIRE(results.hasInstance("f.$@"));
                REQUIRE(results.hasInstance("g.$@"));

                auto historyOfI_isExtendedBy = results.instanceFinderFor("main.i");
                auto historyOfFReturn_isExtendedBy = results.instanceFinderFor("f.$@");

                // Check that i's history includes f
                REQUIRE(historyOfI_isExtendedBy("f.$@"));
                // F's return history dominates j
                CHECK(historyOfFReturn_isExtendedBy("main.j"));
                // And thus, i's history is extended by j
                CHECK(historyOfI_isExtendedBy("main.j"));

                // Check that i's history includes k
                CHECK(historyOfI_isExtendedBy("main.k"));

                auto historyOfGReturn_isExtendedBy = results.instanceFinderFor("g.$@");
                CHECK(historyOfGReturn_isExtendedBy("main.k"));

                // Ensure that cast/pointer operation in return is accounted for
                CHECK(historyOfI_isExtendedBy("h.$@"));
                CHECK(historyOfI_isExtendedBy("h.$1"));

                // Check fptr calls
                auto historyOfF2_0_isExtendedBy = results.instanceFinderFor("f2.$0");
                auto historyOfF2Return_isExtendedBy = results.instanceFinderFor("f2.$@");
                auto historyOfHof_0_isExtendedBy = results.instanceFinderFor("hof.$0");
                auto historyOfHofReturn_isExtendedBy = results.instanceFinderFor("hof.$@");

                auto msgHistory = [&results](auto const &key) {
                    return std::string(">> History for ") + key + ":\n" + results.strInstance(key);
                };

                CHECK_MESSAGE(historyOfI_isExtendedBy("f2.$0"), msgHistory("main.i"));
                CHECK_MESSAGE(historyOfF2_0_isExtendedBy("f2.$@"), msgHistory("f2.$0"));
                //CHECK_MESSAGE(historyOfHof_0_isExtendedBy("f2.$@"), msgHistory("hof.$0"));
                //CHECK_MESSAGE(historyOfHof_0_isExtendedBy("hof.$@"), msgHistory("hof.$0"));
                CHECK_MESSAGE(historyOfF2_0_isExtendedBy("hof.$@"), msgHistory("f2.$0"));
                CHECK_MESSAGE(historyOfF2Return_isExtendedBy("hof.$@"), msgHistory("f2.$@"));
                //CHECK_MESSAGE(historyOfHof_0_isExtendedBy("hof.$1.$@"), msgHistory("hof.$0"));

                //CHECK(historyOfI_isExtendedBy("main.m"));
                CHECK(historyOfHofReturn_isExtendedBy("main.m"));

                // TODO
                // Perhaps this is incorrect
                // This tracking may be useful in some cases even when pointers are not involved.
                // However, this should not lead to wrong alias/cast-based variational type.
                // But non-ptr (value) returns are not aliased
                /*
                CHECK_FALSE(historyOfI_isExtendedBy("main.l"));
                auto historyOfHRet_isExtendedBy = results.instanceFinderFor("h.$@");
                CHECK_FALSE(historyOfHRet_isExtendedBy("main.l"));
                */
            }
        }
    }
}

/*
SCENARIO("Condition-alias") {
    GIVEN("Condition expression involving a reference") {
        // S1: tag comes from a ptr but is not used in case/condition body
        // S2: tag comes from a ptr to struct and is used in case body
        // S3: tag comes from a ptr to struct but is not used in case body
        auto const *code = R"c(
            int FLAG = 0;
            struct Data_t {
                void *data_;
                int type_;
            } last;
            void *last = 0;
            void f(Data *pd) {
                if(pd) {
                    last = pd;
                }

                switch(pd->type_) {
                    case 0: last = pd; break;
                    case 1: (*(int*)pd->data_)++; break;
                }

                Data *p2;
                pd->type_ ? (p2 = pd) : (last = pd);
            }

            int main() {
                int i = 0;
                struct Data d = { &i, 1};
                f(&d);

                return 0;
            }
        )c";

        INFO("Code under test:\n", std::string(code));

        WHEN("cast history is instantiated") {
            auto census = analyze(code);
            TestDB results = std::move(census);
            results.makeInstances();
            CAPTURE_TCCNODES(results.nodes_);

            THEN("condition variable dependency should reflect in history") {
                REQUIRE(results.hasInstance("f.$0"));
                REQUIRE(results.hasInstance("f.$@"));

                // f.$0 include last
            }
        }
    }
}
*/

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
