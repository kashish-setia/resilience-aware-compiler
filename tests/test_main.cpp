#include "api/CompilerApi.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace edge;

static int g_passCount = 0;
static int g_failCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            g_failCount++; \
            return; \
        } \
    } while(0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUN ] " << #fn << "...\n"; \
        int oldFails = g_failCount; \
        fn(); \
        if (g_failCount == oldFails) { \
            std::cout << "  [PASS] " << #fn << "\n"; \
            g_passCount++; \
        } \
    } while(0)

// 1. Lexer Tests
void testLexerValidTokens() {
    DiagnosticEngine diag;
    std::string src = "program P { @critical int x = 42; float y = 3.14; bool b = true; }";
    Lexer lexer(src, diag);
    auto tokens = lexer.tokenize();
    TEST_ASSERT(!diag.hasErrors(), "Lexer should not produce errors for valid tokens");
    TEST_ASSERT(tokens.size() > 10, "Lexer should produce token sequence");
    TEST_ASSERT(tokens[0].kind == TokenKind::KW_PROGRAM, "First token should be program");
}

void testLexerInvalidTokens() {
    DiagnosticEngine diag;
    std::string src = "program P { int x = 10 & 20; }"; // single & is invalid
    Lexer lexer(src, diag);
    lexer.tokenize();
    TEST_ASSERT(diag.hasErrors(), "Lexer should report error on invalid single '&'");
}

// 2. Parser & Syntax Tests
void testParserValidAST() {
    DiagnosticEngine diag;
    std::string src = "program Calc { int a = 10; int b = a + 5 * 2; print(b); }";
    Lexer lexer(src, diag);
    Parser parser(lexer.tokenize(), diag);
    auto ast = parser.parse();
    TEST_ASSERT(!diag.hasErrors() && ast != nullptr, "Parser should succeed on valid program");
    TEST_ASSERT(ast->name == "Calc", "Program name should match");
    TEST_ASSERT(ast->statements.size() == 3, "Program should have 3 statements");
}

void testParserSyntaxError() {
    DiagnosticEngine diag;
    std::string src = "program Broken { int a = ; }"; // Missing expr
    Lexer lexer(src, diag);
    Parser parser(lexer.tokenize(), diag);
    auto ast = parser.parse();
    TEST_ASSERT(diag.hasErrors(), "Parser should report error on missing expression in declaration");
}

void testOperatorPrecedence() {
    // 2 + 3 * 4 should evaluate multiplication first (2 + 12 = 14)
    std::string src = "program Precedence { int res = 2 + 3 * 4; print(res); }";
    auto comp = CompilerApi::compile(src);
    TEST_ASSERT(comp.success, "Precedence program should compile");

    auto sim = CompilerApi::simulate(src);
    TEST_ASSERT(sim.find("\"14\"") != std::string::npos, "Precedence check: 2 + 3 * 4 should equal 14");
}

// 3. Semantic Analysis Tests
void testSemanticUndefinedVariable() {
    DiagnosticEngine diag;
    std::string src = "program Undef { int a = b + 1; }";
    Lexer lexer(src, diag);
    Parser parser(lexer.tokenize(), diag);
    auto ast = parser.parse();
    SemanticAnalyzer sema(diag);
    sema.analyze(*ast);
    TEST_ASSERT(diag.hasErrors(), "Semantic analyzer should catch undefined variable 'b'");
}

void testSemanticDuplicateVariable() {
    DiagnosticEngine diag;
    std::string src = "program Dup { int a = 1; int a = 2; }";
    Lexer lexer(src, diag);
    Parser parser(lexer.tokenize(), diag);
    auto ast = parser.parse();
    SemanticAnalyzer sema(diag);
    sema.analyze(*ast);
    TEST_ASSERT(diag.hasErrors(), "Semantic analyzer should catch duplicate declaration of 'a'");
}

void testSemanticTypeMismatch() {
    DiagnosticEngine diag;
    std::string src = "program Mismatch { bool flag = 42 + 10; }";
    Lexer lexer(src, diag);
    Parser parser(lexer.tokenize(), diag);
    auto ast = parser.parse();
    SemanticAnalyzer sema(diag);
    sema.analyze(*ast);
    TEST_ASSERT(diag.hasErrors(), "Semantic analyzer should reject int to bool assignment");
}

// 4. Control-Flow Graph & Liveness Fixed-Point Tests
void testCFGAndLivenessConvergence() {
    std::string src = 
        "program BranchTest {\n"
        "    int x = 10;\n"
        "    int y = 20;\n"
        "    int z = 0;\n"
        "    if (x > 5) {\n"
        "        z = y * 2;\n"
        "    } else {\n"
        "        z = x + 1;\n"
        "    }\n"
        "    checkpoint();\n"
        "    print(z);\n"
        "}";
    auto comp = CompilerApi::compile(src);
    TEST_ASSERT(comp.success, "BranchTest should compile");
    TEST_ASSERT(comp.cfgJson.find("IfThen") != std::string::npos, "CFG should have Then block");
    TEST_ASSERT(comp.cfgJson.find("IfElse") != std::string::npos, "CFG should have Else block");
    TEST_ASSERT(comp.livenessJson.find("iterations") != std::string::npos, "Liveness should complete fixed point");
}

void testWhileLoopLiveness() {
    std::string src = 
        "program LoopTest {\n"
        "    int i = 0;\n"
        "    int sum = 0;\n"
        "    while (i < 5) {\n"
        "        sum = sum + i;\n"
        "        i = i + 1;\n"
        "    }\n"
        "    checkpoint();\n"
        "    print(sum);\n"
        "}";
    auto comp = CompilerApi::compile(src);
    TEST_ASSERT(comp.success, "LoopTest should compile");
    TEST_ASSERT(comp.cfgJson.find("LoopHeader") != std::string::npos, "CFG should have LoopHeader");
    TEST_ASSERT(comp.cfgJson.find("loop_back") != std::string::npos, "CFG should have loop_back edge");
}

// 5. Four-Way State Classification Tests
void testFourWayClassification() {
    std::string src = 
        "program StateTest {\n"
        "    @critical int sensor_in = read_sensor(0);\n"
        "    int calc = sensor_in + 10;\n"
        "    @optional int cached = calc * 2;\n"
        "    @disposable int debug_cnt = 999;\n"
        "    checkpoint();\n"
        "    print(calc);\n"
        "}";
    auto comp = CompilerApi::compile(src);
    TEST_ASSERT(comp.success, "StateTest should compile");
    TEST_ASSERT(comp.classificationsJson.find("\"CRITICAL\"") != std::string::npos, "Must classify sensor_in as CRITICAL");
    TEST_ASSERT(comp.classificationsJson.find("\"RECOMPUTABLE\"") != std::string::npos, "Must classify calc as RECOMPUTABLE");
    TEST_ASSERT(comp.classificationsJson.find("\"OPTIONAL\"") != std::string::npos, "Must classify cached as OPTIONAL");
    TEST_ASSERT(comp.classificationsJson.find("\"DISPOSABLE\"") != std::string::npos, "Must classify debug_cnt as DISPOSABLE");
}

// 6. Recovery Planner & Budget Constraints Tests
void testRecoveryPlannerSavingsAndBudget() {
    std::string src = 
        "program BudgetTest {\n"
        "    @critical int sensor1 = read_sensor(0);\n"
        "    int rec1 = sensor1 + 5;\n"
        "    int rec2 = rec1 * 2;\n"
        "    @optional int cache1 = 100;\n"
        "    @disposable int dbg = 1;\n"
        "    checkpoint();\n"
        "    print(rec2);\n"
        "}";

    // Unlimited budget plan
    auto compUnlimited = CompilerApi::compile(src, 0);
    TEST_ASSERT(compUnlimited.success, "Compile with unlimited budget should succeed");
    TEST_ASSERT(compUnlimited.recoveryPlanJson.find("\"isFeasible\":true") != std::string::npos, "Plan should be feasible");
    TEST_ASSERT(compUnlimited.recoveryPlanJson.find("\"savingsPercent\"") != std::string::npos, "Plan should calculate savings");

    // Constrained budget that is too small for critical state (sensor1 requires 4 bytes, budget = 2)
    auto compTooSmall = CompilerApi::compile(src, 2);
    TEST_ASSERT(compTooSmall.success, "Compile should succeed");
    TEST_ASSERT(compTooSmall.recoveryPlanJson.find("\"isFeasible\":false") != std::string::npos, 
                "Plan should report infeasibility when budget is strictly smaller than critical state");
}

// 7. Failure Injection, Restoration, and Recovery Correctness
void testSimulationAndRecoveryCorrectness() {
    std::string src = 
        "program RecoveryVerification {\n"
        "    @critical int raw = read_sensor(0);\n" // returns 25 by mock
        "    int offset = 5;\n"
        "    int calibrated = raw + offset; // 30\n"
        "    int doubled = calibrated * 2; // 60\n"
        "    checkpoint();\n"
        "    fault(\"test_crash\");\n"
        "    int final_val = doubled + 10; // 70\n"
        "    print(final_val);\n"
        "}";

    std::string simRes = CompilerApi::simulate(src, CheckpointStrategy::RESILIENCE_AWARE);
    TEST_ASSERT(simRes.find("\"isCorrect\":true") != std::string::npos, "Recovered execution must match reference output exactly");
    TEST_ASSERT(simRes.find("\"70\"") != std::string::npos, "Output should contain expected value 70");
    TEST_ASSERT(simRes.find("\"RECOMPUTE\"") != std::string::npos, "Simulator event log should record RECOMPUTE phase");
}

// 8. Benchmark Strategies Comparison Tests
void testAllFourBenchmarkStrategies() {
    std::string src = 
        "program BenchmarkCheck {\n"
        "    @critical int base = read_sensor(0);\n"
        "    int derived = base * 3;\n"
        "    @optional int cache = derived + 1;\n"
        "    @disposable int dbg = 77;\n"
        "    checkpoint();\n"
        "    print(derived);\n"
        "}";

    std::string benchJson = CompilerApi::runBenchmarks(src);
    TEST_ASSERT(benchJson.find("CHECKPOINT_EVERYTHING") != std::string::npos, "Benchmark must test CHECKPOINT_EVERYTHING");
    TEST_ASSERT(benchJson.find("LIVENESS_ONLY") != std::string::npos, "Benchmark must test LIVENESS_ONLY");
    TEST_ASSERT(benchJson.find("MANUAL_ANNOTATION") != std::string::npos, "Benchmark must test MANUAL_ANNOTATION");
    TEST_ASSERT(benchJson.find("RESILIENCE_AWARE") != std::string::npos, "Benchmark must test RESILIENCE_AWARE");
}

// 9. Built-in Benchmark Programs Validation
void testBuiltinExamplesCompileAndRun() {
    auto examples = CompilerApi::getExamples();
    TEST_ASSERT(examples.size() >= 9, "Must include at least 9 benchmark programs");

    for (const auto& ex : examples) {
        auto comp = CompilerApi::compile(ex.sourceCode);
        TEST_ASSERT(comp.success, "Example '" + ex.id + "' must compile cleanly without errors");

        auto sim = CompilerApi::simulate(ex.sourceCode, CheckpointStrategy::RESILIENCE_AWARE);
        TEST_ASSERT(sim.find("\"isCorrect\":true") != std::string::npos, 
                    "Example '" + ex.id + "' must recover correctly under failure");
    }
}

int main() {
    std::cout << "========================================================\n"
              << "  Running Resilience-Aware Compiler Automated Test Suite\n"
              << "========================================================\n";

    RUN_TEST(testLexerValidTokens);
    RUN_TEST(testLexerInvalidTokens);
    RUN_TEST(testParserValidAST);
    RUN_TEST(testParserSyntaxError);
    RUN_TEST(testOperatorPrecedence);
    RUN_TEST(testSemanticUndefinedVariable);
    RUN_TEST(testSemanticDuplicateVariable);
    RUN_TEST(testSemanticTypeMismatch);
    RUN_TEST(testCFGAndLivenessConvergence);
    RUN_TEST(testWhileLoopLiveness);
    RUN_TEST(testFourWayClassification);
    RUN_TEST(testRecoveryPlannerSavingsAndBudget);
    RUN_TEST(testSimulationAndRecoveryCorrectness);
    RUN_TEST(testAllFourBenchmarkStrategies);
    RUN_TEST(testBuiltinExamplesCompileAndRun);

    std::cout << "\n========================================================\n"
              << "  Test Suite Summary: " << g_passCount << " Passed, " << g_failCount << " Failed\n"
              << "========================================================\n";

    return (g_failCount == 0) ? 0 : 1;
}
