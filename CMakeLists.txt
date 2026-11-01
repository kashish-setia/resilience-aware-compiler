cmake_minimum_required(VERSION 3.20)
project(ResilienceAwareCompiler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and warning flags
if(MSVC)
    add_compile_options(/W4 /EHsc /O2)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/compiler/include
)

# Compiler core library sources
set(LIB_SOURCES
    compiler/src/lexer/Token.cpp
    compiler/src/lexer/Lexer.cpp
    compiler/src/parser/AST.cpp
    compiler/src/parser/Parser.cpp
    compiler/src/semantic/SymbolTable.cpp
    compiler/src/semantic/SemanticAnalyzer.cpp
    compiler/src/cfg/CFG.cpp
    compiler/src/cfg/CFGBuilder.cpp
    compiler/src/analysis/LivenessAnalysis.cpp
    compiler/src/analysis/DefUseAnalysis.cpp
    compiler/src/analysis/DependencyGraph.cpp
    compiler/src/classifier/StateClassifier.cpp
    compiler/src/planner/RecoveryPlanner.cpp
    compiler/src/ir/Bytecode.cpp
    compiler/src/ir/CodeGenerator.cpp
    compiler/src/runtime/VirtualMachine.cpp
    compiler/src/runtime/CheckpointStore.cpp
    compiler/src/runtime/FailureSimulator.cpp
    compiler/src/runtime/BenchmarkRunner.cpp
    compiler/src/api/CompilerApi.cpp
    compiler/src/api/HttpServer.cpp
)

add_library(edge_compiler_lib STATIC ${LIB_SOURCES})

# Windows socket library link
if(WIN32)
    target_link_libraries(edge_compiler_lib PUBLIC ws2_32)
endif()

# CLI Executable
add_executable(edgec compiler/src/main.cpp)
target_link_libraries(edgec PRIVATE edge_compiler_lib)

# Automated Test Suite
enable_testing()
add_executable(edge_test_suite tests/test_main.cpp)
target_link_libraries(edge_test_suite PRIVATE edge_compiler_lib)
add_test(NAME AllCompilerTests COMMAND edge_test_suite)
