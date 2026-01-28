cmake_minimum_required(VERSION 3.15)
project(KnowledgeGraphExtractor VERSION 1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ==============================================================================
# Dependencies
# ==============================================================================

# Find required packages
find_package(CURL REQUIRED)

# nlohmann_json (header-only, use FetchContent if not found)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.2
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Poppler (for PDF processing) - optional for now
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(POPPLER poppler-cpp)
    if(POPPLER_FOUND)
        set(HAVE_POPPLER ON)
        add_definitions(-DHAVE_POPPLER)
    else()
        message(WARNING "Poppler not found. PDF processing will be limited.")
    endif()
else()
    message(WARNING "pkg-config not found. Poppler detection skipped.")
endif()

# ==============================================================================
# Include directories
# ==============================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CURL_INCLUDE_DIRS}
)

if(HAVE_POPPLER)
    include_directories(${POPPLER_INCLUDE_DIRS})
    link_directories(${POPPLER_LIBRARY_DIRS})
endif()

# ==============================================================================
# Hypergraph Library
# ==============================================================================

add_library(hypergraph
    src/graph/hypergraph.cpp
    src/graph/hypergraph_extended.cpp
)

target_include_directories(hypergraph PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(hypergraph PUBLIC
    nlohmann_json::nlohmann_json
)

# ==============================================================================
# PDF Processing Library
# ==============================================================================

if(HAVE_POPPLER)
    add_library(pdf_processor
        src/pdf/pdf_processor.cpp
    )

    target_include_directories(pdf_processor PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${POPPLER_INCLUDE_DIRS}
    )

    target_link_libraries(pdf_processor PUBLIC
        ${POPPLER_LIBRARIES}
    )
else()
    message(STATUS "Poppler not found, PDF processing will not be available")
endif()

# ==============================================================================
# LLM Provider Library
# ==============================================================================

add_library(llm_provider
    src/llm/llm_provider.cpp
)

target_include_directories(llm_provider PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(llm_provider PUBLIC
    ${CURL_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# ==============================================================================
# Extraction Pipeline Library
# ==============================================================================

add_library(extraction_pipeline
    src/pipeline/extraction_pipeline.cpp
)

target_include_directories(extraction_pipeline PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(extraction_pipeline PUBLIC
    hypergraph
    llm_provider
    nlohmann_json::nlohmann_json
)

# Add PDF processor if available
if(HAVE_POPPLER)
    target_link_libraries(extraction_pipeline PUBLIC
        pdf_processor
    )
endif()

# ==============================================================================
# Discovery Library
# ==============================================================================

add_library(discovery
    src/discovery/discovery_engine.cpp
    src/discovery/report_generator.cpp
    src/render/augmentation_renderer.cpp
)

target_include_directories(discovery PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(discovery PUBLIC
    hypergraph
    nlohmann_json::nlohmann_json
)

# ==============================================================================
# Knowledge Graph CLI (kg)
# ==============================================================================

add_executable(kg
    src/main.cpp
)

target_link_libraries(kg PRIVATE
    hypergraph
    discovery
    extraction_pipeline
    nlohmann_json::nlohmann_json
)

# ==============================================================================
# Example executables
# ==============================================================================

# Hypergraph example
add_executable(hypergraph_example
    examples/hypergraph_example.cpp
)

target_link_libraries(hypergraph_example PRIVATE
    hypergraph
)

# Historical knowledge example (from tests/1page.pdf)
add_executable(historical_knowledge_example
    examples/historical_knowledge_example.cpp
)

target_link_libraries(historical_knowledge_example PRIVATE
    hypergraph
)

# PDF processing example (requires Poppler)
if(HAVE_POPPLER)
    add_executable(pdf_processing_example
        examples/pdf_processing_example.cpp
    )

    target_link_libraries(pdf_processing_example PRIVATE
        pdf_processor
    )
else()
    message(STATUS "Skipping pdf_processing_example (requires Poppler)")
endif()

# LLM extraction example
add_executable(llm_extraction_example
    examples/llm_extraction_example.cpp
)

target_link_libraries(llm_extraction_example PRIVATE
    llm_provider
    hypergraph
)

# LLM extraction example with PDF support
if(HAVE_POPPLER)
    target_link_libraries(llm_extraction_example PRIVATE
        pdf_processor
    )
endif()

# End-to-end pipeline example
add_executable(pipeline_example
    examples/pipeline_example.cpp
)

target_link_libraries(pipeline_example PRIVATE
    extraction_pipeline
)

# JSON to HTML converter utility
add_executable(json_to_html
    examples/json_to_html.cpp
)

target_link_libraries(json_to_html PRIVATE
    hypergraph
)

# ==============================================================================
# Original POC (from Hypergraph.cpp)
# ==============================================================================

if(HAVE_POPPLER)
    add_executable(hg_agent_poc
        References/Hypergraph.cpp
    )

    target_include_directories(hg_agent_poc PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${POPPLER_INCLUDE_DIRS}
    )

    target_link_libraries(hg_agent_poc PRIVATE
        ${CURL_LIBRARIES}
        ${POPPLER_LIBRARIES}
        nlohmann_json::nlohmann_json
    )
else()
    message(STATUS "Skipping hg_agent_poc (requires Poppler)")
endif()

# ==============================================================================
# Testing
# ==============================================================================

option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()

    # Try to find Google Test
    find_package(GTest QUIET)

    if(GTest_FOUND)
        message(STATUS "Google Test found, building tests")

        # Unit tests
        add_executable(test_hypergraph
            tests/unit/test_hypergraph.cpp
        )

        target_link_libraries(test_hypergraph PRIVATE
            hypergraph
            GTest::gtest
            GTest::gtest_main
        )

        add_test(NAME HypergraphTests COMMAND test_hypergraph)

    else()
        message(STATUS "Google Test not found, tests will not be built")
        message(STATUS "To install: sudo apt-get install libgtest-dev")
    endif()
endif()

# ==============================================================================
# Installation
# ==============================================================================

install(TARGETS hypergraph
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS hypergraph_example
    RUNTIME DESTINATION bin
)

# ==============================================================================
# Documentation
# ==============================================================================

option(BUILD_DOCS "Build documentation" OFF)

if(BUILD_DOCS)
    find_package(Doxygen)
    if(DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/Doxyfile)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(docs ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    else()
        message(WARNING "Doxygen not found, documentation will not be built")
    endif()
endif()

# ==============================================================================
# Summary
# ==============================================================================

message(STATUS "")
message(STATUS "=== Knowledge Graph Extractor Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  CURL: ${CURL_FOUND}")
message(STATUS "  nlohmann_json: ${nlohmann_json_FOUND}")
message(STATUS "  Poppler: ${HAVE_POPPLER}")
message(STATUS "  GTest: ${GTest_FOUND}")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  Hypergraph library: ON")
message(STATUS "  Examples: ON")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Documentation: ${BUILD_DOCS}")
message(STATUS "")
message(STATUS "Executables:")
message(STATUS "  kg (CLI): ON")
message(STATUS "  hypergraph_example: ON")
message(STATUS "  json_to_html: ON")
if(HAVE_POPPLER)
    message(STATUS "  hg_agent_poc: ON")
    message(STATUS "  pipeline_example: ON")
else()
    message(STATUS "  hg_agent_poc: OFF (requires Poppler)")
    message(STATUS "  pipeline_example: OFF (requires Poppler)")
endif()
message(STATUS "")
message(STATUS "===============================================")
message(STATUS "")
