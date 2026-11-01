#include "api/CompilerApi.h"
#include <sstream>
#include <chrono>

namespace edge {

static std::string escapeJson(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\t') res += "\\t";
        else res += c;
    }
    return res;
}

std::string CompilationResult::toJson() const {
    std::ostringstream oss;
    oss << "{"
        << "\"success\":" << (success ? "true" : "false") << ","
        << "\"programName\":\"" << escapeJson(programName) << "\","
        << "\"compilationTimeMs\":" << compilationTimeMs << ","
        << "\"diagnostics\":" << (diagnosticsJson.empty() ? "[]" : diagnosticsJson) << ","
        << "\"tokens\":" << (tokensJson.empty() ? "[]" : tokensJson) << ","
        << "\"ast\":" << (astJson.empty() ? "null" : astJson) << ","
        << "\"symbolTable\":" << (symbolTableJson.empty() ? "[]" : symbolTableJson) << ","
        << "\"cfg\":" << (cfgJson.empty() ? "null" : cfgJson) << ","
        << "\"liveness\":" << (livenessJson.empty() ? "null" : livenessJson) << ","
        << "\"defUse\":" << (defUseJson.empty() ? "null" : defUseJson) << ","
        << "\"dependencyGraph\":" << (dependencyJson.empty() ? "null" : dependencyJson) << ","
        << "\"classifications\":" << (classificationsJson.empty() ? "[]" : classificationsJson) << ","
        << "\"recoveryPlan\":" << (recoveryPlanJson.empty() ? "null" : recoveryPlanJson) << ","
        << "\"bytecode\":" << (bytecodeJson.empty() ? "null" : bytecodeJson) << ","
        << "\"disassembled\":\"" << escapeJson(disassembled) << "\""
        << "}";
    return oss.str();
}

std::string ExampleProgram::toJson() const {
    std::ostringstream oss;
    oss << "{\"id\":\"" << escapeJson(id) << "\","
        << "\"title\":\"" << escapeJson(title) << "\","
        << "\"description\":\"" << escapeJson(description) << "\","
        << "\"category\":\"" << escapeJson(category) << "\","
        << "\"sourceCode\":\"" << escapeJson(sourceCode) << "\"}";
    return oss.str();
}

CompilationResult CompilerApi::compile(
    const std::string& sourceCode,
    size_t memoryBudgetBytes,
    const std::string& filename
) {
    auto startTime = std::chrono::high_resolution_clock::now();

    CompilationResult res;
    DiagnosticEngine diag;

    // 1. Lexer
    Lexer lexer(sourceCode, diag, filename);
    std::vector<Token> tokens = lexer.tokenize();

    std::ostringstream tokOss;
    tokOss << "[";
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) tokOss << ",";
        tokOss << tokens[i].toJson();
    }
    tokOss << "]";
    res.tokensJson = tokOss.str();

    if (diag.hasErrors()) {
        res.success = false;
        res.diagnosticsJson = diag.toJson();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.compilationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    // 2. Parser
    Parser parser(tokens, diag);
    auto ast = parser.parse();

    if (diag.hasErrors() || !ast) {
        res.success = false;
        res.diagnosticsJson = diag.toJson();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.compilationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    res.programName = ast->name;
    res.astJson = ast->toJson();

    // 3. Semantic Analyzer
    SemanticAnalyzer sema(diag);
    sema.analyze(*ast);
    res.symbolTableJson = sema.getSymbolTable().toJson();

    if (diag.hasErrors()) {
        res.success = false;
        res.diagnosticsJson = diag.toJson();
        auto endTime = std::chrono::high_resolution_clock::now();
        res.compilationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
        return res;
    }

    // 4. CFG Construction
    CFGBuilder cfgBuilder;
    auto cfg = cfgBuilder.build(*ast);
    res.cfgJson = cfg->toJson();

    // 5. Liveness Analysis
    LivenessAnalysis liveness(*cfg);
    liveness.run();
    res.livenessJson = liveness.toJson();

    // 6. Def-Use Analysis
    DefUseAnalysis defUse;
    defUse.analyze(*ast);
    res.defUseJson = defUse.toJson();

    // 7. Value Dependency Graph
    DependencyGraph depGraph;
    depGraph.build(*ast, sema.getSymbolTable());
    res.dependencyJson = depGraph.toJson();

    // 8. Four-Way State Classification
    StateClassifier classifier(sema.getSymbolTable(), liveness, depGraph);
    auto classifications = classifier.classifyVariablesAtCheckpoint(1);
    res.classificationsJson = classifier.toJson(classifications);

    // 9. Recovery Planner
    RecoveryPlanner planner(depGraph);
    CheckpointPlan plan = planner.plan(1, classifications, CheckpointStrategy::RESILIENCE_AWARE, memoryBudgetBytes);
    res.recoveryPlanJson = plan.toJson();

    // 10. Code Generation (Bytecode)
    CodeGenerator codegen;
    auto bytecode = codegen.generate(*ast);
    res.bytecodeJson = bytecode->toJson();
    res.disassembled = bytecode->disassemble();

    res.success = true;
    res.diagnosticsJson = diag.toJson();

    auto endTime = std::chrono::high_resolution_clock::now();
    res.compilationTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    return res;
}

std::string CompilerApi::simulate(
    const std::string& sourceCode,
    CheckpointStrategy strategy,
    size_t memoryBudgetBytes,
    int32_t faultCycle,
    size_t faultStatementId
) {
    DiagnosticEngine diag;
    Lexer lexer(sourceCode, diag);
    auto tokens = lexer.tokenize();
    if (diag.hasErrors()) return "{\"error\":\"Lexical errors detected\"}";

    Parser parser(tokens, diag);
    auto ast = parser.parse();
    if (diag.hasErrors() || !ast) return "{\"error\":\"Syntax errors detected\"}";

    SemanticAnalyzer sema(diag);
    if (!sema.analyze(*ast)) return "{\"error\":\"Semantic errors detected\"}";

    CFGBuilder cfgBuilder;
    auto cfg = cfgBuilder.build(*ast);
    LivenessAnalysis liveness(*cfg);
    liveness.run();

    DependencyGraph depGraph;
    depGraph.build(*ast, sema.getSymbolTable());

    StateClassifier classifier(sema.getSymbolTable(), liveness, depGraph);
    auto classifications = classifier.classifyVariablesAtCheckpoint(1);

    RecoveryPlanner planner(depGraph);
    CheckpointPlan plan = planner.plan(1, classifications, strategy, memoryBudgetBytes);

    CodeGenerator codegen;
    auto bytecode = codegen.generate(*ast);

    FailureSimulator simulator(*bytecode, depGraph);
    SimulationResult simRes = simulator.run(plan, faultCycle, faultStatementId);

    return simRes.toJson();
}

std::string CompilerApi::runBenchmarks(const std::string& sourceCode) {
    std::vector<ExampleProgram> programs;
    if (!sourceCode.empty()) {
        programs.push_back({"custom", "Active Program", "Custom user-supplied program", "Custom", sourceCode});
    } else {
        programs = getExamples();
    }

    std::ostringstream oss;
    oss << "[";

    for (size_t i = 0; i < programs.size(); ++i) {
        if (i > 0) oss << ",";
        const auto& p = programs[i];

        DiagnosticEngine diag;
        Lexer lexer(p.sourceCode, diag);
        auto tokens = lexer.tokenize();
        Parser parser(tokens, diag);
        auto ast = parser.parse();
        SemanticAnalyzer sema(diag);
        sema.analyze(*ast);

        CFGBuilder cfgBuilder;
        auto cfg = cfgBuilder.build(*ast);
        LivenessAnalysis liveness(*cfg);
        liveness.run();

        DependencyGraph depGraph;
        depGraph.build(*ast, sema.getSymbolTable());

        StateClassifier classifier(sema.getSymbolTable(), liveness, depGraph);
        auto classifications = classifier.classifyVariablesAtCheckpoint(1);

        CodeGenerator codegen;
        auto bytecode = codegen.generate(*ast);

        BenchmarkSuiteResult res = BenchmarkRunner::runBenchmark(
            p.title, p.description, *ast, depGraph, classifications, *bytecode, 0.4
        );

        oss << res.toJson();
    }

    oss << "]";
    return oss.str();
}

std::vector<ExampleProgram> CompilerApi::getExamples() {
    return {
        {
            "temperature_logger",
            "Temperature Monitor",
            "Continuous temperature logger reading sensor, applying calibration, scaling, and threshold alert.",
            "Edge Sensor",
            "program TemperatureMonitor {\n"
            "    @critical int raw = read_sensor(0);\n"
            "    int calibration = 3;\n"
            "    int adjusted = raw + calibration;\n"
            "    int filtered = adjusted * 2;\n"
            "    int threshold = 50;\n"
            "    @optional int cached_status = 1;\n"
            "    @disposable int debug_tick = 99;\n"
            "    checkpoint();\n"
            "    bool alert = filtered > threshold;\n"
            "    print(alert);\n"
            "}"
        },
        {
            "sensor_calibration",
            "Sensor Calibration Pipeline",
            "Industrial sensor zero-offset calibration, linear gain multiplication, and range normalization.",
            "Calibration",
            "program SensorCalibration {\n"
            "    @critical int raw_in = read_sensor(0);\n"
            "    int zero_offset = 10;\n"
            "    int gain_multiplier = 4;\n"
            "    int calibrated = (raw_in - zero_offset) * gain_multiplier;\n"
            "    @optional int cached_average = calibrated / 2;\n"
            "    @disposable int debug_seq = 1001;\n"
            "    checkpoint();\n"
            "    int normalized = calibrated + 5;\n"
            "    print(normalized);\n"
            "}"
        },
        {
            "heart_rate_monitor",
            "Heart-Rate Threshold Monitor",
            "Medical monitoring node checking for bradycardia and tachycardia conditions with mock telemetry.",
            "Healthcare",
            "program HeartRateMonitor {\n"
            "    @critical int pulse_raw = read_sensor(1);\n"
            "    int baseline = 60;\n"
            "    int delta = pulse_raw - baseline;\n"
            "    int bpm = baseline + delta;\n"
            "    bool tachycardia = bpm > 100;\n"
            "    bool bradycardia = bpm < 50;\n"
            "    @optional int cache_zone = 2;\n"
            "    checkpoint();\n"
            "    print(bpm);\n"
            "    print(tachycardia);\n"
            "}"
        },
        {
            "arithmetic_chain",
            "Arithmetic Dependency Chain",
            "Deep multi-step arithmetic transformation demonstrating dependency reconstruction ordering.",
            "Compute-Heavy",
            "program ArithmeticChain {\n"
            "    @critical int start_seed = read_sensor(0);\n"
            "    int step1 = start_seed + 10;\n"
            "    int step2 = step1 * 3;\n"
            "    int step3 = step2 - 15;\n"
            "    int step4 = step3 / 2;\n"
            "    int step5 = step4 + 7;\n"
            "    @disposable int temp_calc = 888;\n"
            "    checkpoint();\n"
            "    int final_result = step5 * 2;\n"
            "    print(final_result);\n"
            "}"
        },
        {
            "conditional_branches",
            "Conditional Environmental Control",
            "Multi-path branching control adjusting actuation states according to environmental temperature.",
            "Control Logic",
            "program EnvironmentalControl {\n"
            "    @critical int ambient = read_sensor(0);\n"
            "    int target = 22;\n"
            "    int mode = 0;\n"
            "    if (ambient > target) {\n"
            "        mode = 1;\n"
            "    } else {\n"
            "        mode = 2;\n"
            "    }\n"
            "    @optional int cache_power = mode * 50;\n"
            "    checkpoint();\n"
            "    print(mode);\n"
            "}"
        },
        {
            "loop_accumulator",
            "Loop-Based Energy Accumulator",
            "Iterative loop accumulating sensor values over multiple cycles before checkpoint preservation.",
            "Loops",
            "program LoopAccumulator {\n"
            "    @critical int sensor_base = read_sensor(0);\n"
            "    int sum = 0;\n"
            "    int i = 0;\n"
            "    while (i < 5) {\n"
            "        sum = sum + sensor_base;\n"
            "        i = i + 1;\n"
            "    }\n"
            "    @disposable int loop_counter_debug = i;\n"
            "    checkpoint();\n"
            "    print(sum);\n"
            "}"
        },
        {
            "disposable_debug",
            "Telemetry Logger with Debug State",
            "Edge node with temporary volatile debug state and metadata intended to be discarded across power loss.",
            "Disposable State",
            "program TelemetryLogger {\n"
            "    @critical int telemetry_packet = read_sensor(2);\n"
            "    int checksum = telemetry_packet % 256;\n"
            "    @disposable int log_id = 9999;\n"
            "    @disposable int volatile_status = 123;\n"
            "    @disposable int debug_timestamp = 45678;\n"
            "    checkpoint();\n"
            "    print(checksum);\n"
            "}"
        },
        {
            "expensive_recompute",
            "Polynomial Computation Pipeline",
            "Program containing mathematically intensive intermediate polynomial coefficients safe to recompute.",
            "Optimization",
            "program PolynomialPipeline {\n"
            "    @critical int input_x = read_sensor(0);\n"
            "    int poly1 = input_x * input_x;\n"
            "    int poly2 = poly1 * 3;\n"
            "    int poly3 = poly2 + (input_x * 5);\n"
            "    int curve_val = poly3 + 17;\n"
            "    checkpoint();\n"
            "    int output_metric = curve_val / 4;\n"
            "    print(output_metric);\n"
            "}"
        },
        {
            "external_sensor_io",
            "Multi-Sensor Fusion Node",
            "Multiple physical hardware sensor inputs (temperature & pressure) that must strictly be preserved.",
            "Multi-Sensor",
            "program MultiSensorFusion {\n"
            "    @critical int temp_sensor = read_sensor(0);\n"
            "    @critical int pressure_sensor = read_sensor(2);\n"
            "    int combined_index = (temp_sensor * 10) + (pressure_sensor / 100);\n"
            "    @optional int cache_reading = combined_index;\n"
            "    checkpoint();\n"
            "    print(combined_index);\n"
            "}"
        }
    };
}

} // namespace edge
