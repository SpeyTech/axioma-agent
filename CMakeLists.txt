# CMakeLists.txt
# axioma-agent — Layer 5 Behavioural Agent
#
# DVEC: v1.3
# DETERMINISM: D2 — Constrained Deterministic
# MEMORY: Zero Dynamic Allocation
#
# Copyright (c) 2026 The Murray Family Innovation Trust
# SPDX-License-Identifier: GPL-3.0-or-later
# Patent: UK GB2521625.0
#
# @traceability SRS-002-SHALL-018

cmake_minimum_required(VERSION 3.10)

project(axioma-agent
    VERSION 0.1.0
    LANGUAGES C
    DESCRIPTION "Axilog Layer 5 Behavioural Agent"
)

# ==============================================================================
# C STANDARD CONFIGURATION
# ==============================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ==============================================================================
# COMPILER FLAGS (SRS-002-SHALL-018: Bounded execution)
# ==============================================================================

# Core warning flags — zero warnings required
add_compile_options(
    -Wall
    -Wextra
    -Werror
    -pedantic
    -Wshadow
    -Wconversion
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wold-style-definition
    -Wpointer-arith
    -Wcast-qual
    -Wcast-align
    -Wwrite-strings
    -Wformat=2
    -Wformat-security
    -Wstrict-overflow=5
    -Wundef
    -Wredundant-decls
    -Wnested-externs
    -Winline
    -Wno-unused-parameter
)

# Stack protection
add_compile_options(-fstack-protector-strong)

# Disable floating-point optimizations that could affect determinism
add_compile_options(
    -ffp-contract=off
    -fno-fast-math
)

# ==============================================================================
# DETERMINISM CLOSURE FLAGS (Phase 1.5 aligned)
# ==============================================================================
# These flags remove compiler optimisation-induced variance and align behaviour
# across toolchains. Required for D2 certification claims.
#
# @traceability DVEC-001 Section 4.1 (Determinism Contract)

add_compile_options(
    # Prevent type-punning optimisations that assume strict aliasing
    -fno-strict-aliasing
    
    # Define signed overflow as wrapping (two's complement)
    -fwrapv
    
    # Disable auto-vectorisation (SIMD variance across platforms)
    -fno-tree-vectorize
    
    # Use explicit implementations, not compiler builtins
    -fno-builtin
    
    # Preserve frame pointer for debugging and stack traces
    -fno-omit-frame-pointer
)

# Platform-specific flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fno-common)
endif()

# ==============================================================================
# INCLUDE DIRECTORIES
# ==============================================================================

# axioma-agent owns include/axilog/agent.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# axioma-spec owns types.h, commitment.h, dvm_compat.h, dvec.h
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../axioma-spec/include)

# axioma-audit owns audit.h (L6 substrate)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../axioma-audit/include)

# axioma-audit internal headers (sha256_internal.h)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../axioma-audit/src)

# ==============================================================================
# DEPENDENCIES
# ==============================================================================

# Link against axioma-audit (L6 substrate)
# Build axioma-audit first if not already built
if(NOT TARGET axioma-audit)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../axioma-audit 
                     ${CMAKE_BINARY_DIR}/axioma-audit)
endif()

# ==============================================================================
# SOURCE FILES
# ==============================================================================

set(AXIOMA_AGENT_SOURCES
    src/agent.c
)

# ==============================================================================
# LIBRARY TARGET
# ==============================================================================

add_library(axioma-agent STATIC ${AXIOMA_AGENT_SOURCES})

target_link_libraries(axioma-agent axioma-audit)

target_include_directories(axioma-agent PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../axioma-spec/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../axioma-audit/include>
    $<INSTALL_INTERFACE:include>
)

# ==============================================================================
# TEST TARGETS
# ==============================================================================

enable_testing()

# Test: agent totality
add_executable(test_agent_totality tests/identity/test_agent_totality.c)
target_link_libraries(test_agent_totality axioma-agent)
add_test(NAME test_agent_totality COMMAND test_agent_totality)

# ==============================================================================
# CUSTOM TARGETS
# ==============================================================================

# Run all tests
add_custom_target(agent-test-all
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS test_agent_totality
    COMMENT "Running all axioma-agent tests"
)

# ==============================================================================
# INSTALLATION (optional)
# ==============================================================================

install(TARGETS axioma-agent
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/axilog/
    DESTINATION include/axilog
    FILES_MATCHING PATTERN "*.h"
)

# ==============================================================================
# SUMMARY
# ==============================================================================

message(STATUS "")
message(STATUS "axioma-agent configuration:")
message(STATUS "  Version:     ${PROJECT_VERSION}")
message(STATUS "  C Standard:  C${CMAKE_C_STANDARD}")
message(STATUS "  Build Type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:    ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "")
