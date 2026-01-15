#!/bin/bash

# Build script for Knowledge Graph Extractor
# Usage: ./build.sh [clean|test|install|help]

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print colored message
print_msg() {
    echo -e "${2}${1}${NC}"
}

# Check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check dependencies
check_dependencies() {
    print_msg "Checking dependencies..." "$BLUE"

    if ! command_exists cmake; then
        print_msg "❌ CMake not found. Install with: sudo apt-get install cmake" "$RED"
        exit 1
    fi

    if ! command_exists g++; then
        print_msg "❌ g++ not found. Install with: sudo apt-get install g++" "$RED"
        exit 1
    fi

    if ! pkg-config --exists libcurl; then
        print_msg "⚠️  libcurl not found. Install with: sudo apt-get install libcurl4-openssl-dev" "$YELLOW"
    fi

    if ! pkg-config --exists poppler-cpp; then
        print_msg "⚠️  poppler-cpp not found. PDF support will be limited." "$YELLOW"
        print_msg "   Install with: sudo apt-get install libpoppler-cpp-dev" "$YELLOW"
    fi

    print_msg "✓ Required dependencies found" "$GREEN"
}

# Clean build directory
clean_build() {
    print_msg "Cleaning build directory..." "$BLUE"
    rm -rf build
    print_msg "✓ Clean complete" "$GREEN"
}

# Build project
build_project() {
    print_msg "Building project..." "$BLUE"

    # Create build directory
    mkdir -p build
    cd build

    # Configure
    print_msg "Configuring with CMake..." "$BLUE"
    cmake .. "$@"

    # Build
    print_msg "Compiling..." "$BLUE"
    make -j$(nproc)

    cd ..
    print_msg "✓ Build complete" "$GREEN"
}

# Run tests
run_tests() {
    print_msg "Running tests..." "$BLUE"

    if [ ! -d "build" ]; then
        print_msg "Build directory not found. Building first..." "$YELLOW"
        build_project
    fi

    cd build
    if [ -f "bin/test_hypergraph" ]; then
        ./bin/test_hypergraph
        print_msg "✓ Tests passed" "$GREEN"
    else
        print_msg "⚠️  Tests not built. Install Google Test and rebuild." "$YELLOW"
    fi
    cd ..
}

# Install
install_project() {
    print_msg "Installing..." "$BLUE"

    if [ ! -d "build" ]; then
        print_msg "Build directory not found. Building first..." "$YELLOW"
        build_project
    fi

    cd build
    sudo make install
    cd ..

    print_msg "✓ Installation complete" "$GREEN"
}

# Run example
run_example() {
    print_msg "Running hypergraph example..." "$BLUE"

    if [ ! -d "build" ]; then
        print_msg "Build directory not found. Building first..." "$YELLOW"
        build_project
    fi

    cd build
    if [ -f "bin/hypergraph_example" ]; then
        ./bin/hypergraph_example
    else
        print_msg "❌ Example not found. Build failed?" "$RED"
        exit 1
    fi
    cd ..
}

# Show help
show_help() {
    cat << EOF
Knowledge Graph Extractor - Build Script

Usage: ./build.sh [command]

Commands:
    build       Build the project (default)
    clean       Clean build directory
    rebuild     Clean and rebuild
    test        Build and run tests
    example     Build and run example
    install     Install to system
    check       Check dependencies
    help        Show this help message

Build Options:
    debug       Build in debug mode
    release     Build in release mode (default)

Examples:
    ./build.sh              # Build project
    ./build.sh clean        # Clean build directory
    ./build.sh rebuild      # Clean and rebuild
    ./build.sh test         # Run tests
    ./build.sh example      # Run example
    ./build.sh debug build  # Debug build

EOF
}

# Main script
main() {
    local cmd=${1:-build}
    local build_type=${2:-Release}

    case $cmd in
        build)
            check_dependencies
            build_project -DCMAKE_BUILD_TYPE=$build_type
            ;;
        clean)
            clean_build
            ;;
        rebuild)
            clean_build
            check_dependencies
            build_project -DCMAKE_BUILD_TYPE=$build_type
            ;;
        test)
            check_dependencies
            build_project -DCMAKE_BUILD_TYPE=$build_type -DBUILD_TESTS=ON
            run_tests
            ;;
        example)
            check_dependencies
            build_project -DCMAKE_BUILD_TYPE=$build_type
            run_example
            ;;
        install)
            install_project
            ;;
        check)
            check_dependencies
            ;;
        debug)
            check_dependencies
            build_project -DCMAKE_BUILD_TYPE=Debug
            ;;
        release)
            check_dependencies
            build_project -DCMAKE_BUILD_TYPE=Release
            ;;
        help)
            show_help
            ;;
        *)
            print_msg "Unknown command: $cmd" "$RED"
            show_help
            exit 1
            ;;
    esac
}

# Run main
main "$@"
