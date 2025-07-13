#!/bin/bash

# Setup and Build Script for End-to-End Tests
# Video Simili Duplicate Cleaner - E2E Tests

set -e  # Exit on any error

echo "=== Video Simili Duplicate Cleaner - E2E Tests Setup ==="
echo

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install dependencies on Linux
install_linux_deps() {
    print_status "Installing dependencies for Linux..."
    
    # Update package list
    sudo apt update
    
    # Install build essentials
    sudo apt install -y cmake build-essential
    
    # Install Qt5 development packages
    sudo apt install -y qtbase5-dev qtbase5-dev-tools libqt5sql5-sqlite
    
    # Install Google Test
    sudo apt install -y libgtest-dev libgmock-dev
    
    # Install OpenCV
    sudo apt install -y libopencv-dev
    
    # Install FFmpeg
    sudo apt install -y ffmpeg libavcodec-dev libavformat-dev libswscale-dev libavutil-dev libswresample-dev
    
    print_success "Linux dependencies installed successfully"
}

# Function to install dependencies on macOS
install_macos_deps() {
    print_status "Installing dependencies for macOS..."
    
    # Check if Homebrew is installed
    if ! command_exists brew; then
        print_error "Homebrew is not installed. Please install it first:"
        print_error "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi
    
    # Install dependencies
    brew install cmake
    brew install qt@5
    brew install googletest
    brew install opencv
    brew install ffmpeg
    
    # Set up Qt5 environment
    echo "export Qt5_DIR=/usr/local/opt/qt@5/lib/cmake/Qt5" >> ~/.zshrc
    echo "export PATH=\"/usr/local/opt/qt@5/bin:\$PATH\"" >> ~/.zshrc
    
    print_success "macOS dependencies installed successfully"
    print_warning "Please restart your terminal or run 'source ~/.zshrc' to apply Qt5 environment variables"
}

# Function to check if all dependencies are available
check_dependencies() {
    print_status "Checking dependencies..."
    
    local missing_deps=()
    
    # Check for CMake
    if ! command_exists cmake; then
        missing_deps+=("cmake")
    fi
    
    # Check for Qt5
    if ! pkg-config --exists Qt5Core; then
        missing_deps+=("Qt5")
    fi
    
    # Check for Google Test (this is a bit tricky to check properly)
    if ! ldconfig -p | grep -q libgtest; then
        if [[ ! -f /usr/lib/libgtest.a ]] && [[ ! -f /usr/local/lib/libgtest.a ]]; then
            missing_deps+=("Google Test")
        fi
    fi
    
    # Check for OpenCV
    if ! pkg-config --exists opencv4; then
        if ! pkg-config --exists opencv; then
            missing_deps+=("OpenCV")
        fi
    fi
    
    # Check for FFmpeg
    if ! pkg-config --exists libavcodec; then
        missing_deps+=("FFmpeg")
    fi
    
    if [ ${#missing_deps[@]} -eq 0 ]; then
        print_success "All dependencies are available"
        return 0
    else
        print_error "Missing dependencies: ${missing_deps[*]}"
        return 1
    fi
}

# Function to build the tests
build_tests() {
    print_status "Building end-to-end tests..."
    
    # Create build directory
    if [ -d "build" ]; then
        print_status "Removing existing build directory..."
        rm -rf build
    fi
    
    mkdir build
    cd build
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Debug
    
    # Build
    print_status "Building tests..."
    make -j$(nproc 2>/dev/null || echo 4)
    
    # Check if executable was created
    if [ -f "video-simili-duplicate-cleaner-e2e-tests" ]; then
        print_success "Tests built successfully!"
        return 0
    else
        print_error "Build failed - executable not found"
        return 1
    fi
}

# Function to run tests
run_tests() {
    print_status "Running end-to-end tests..."
    
    if [ ! -f "video-simili-duplicate-cleaner-e2e-tests" ]; then
        print_error "Test executable not found. Please build first."
        exit 1
    fi
    
    # Run all tests
    ./video-simili-duplicate-cleaner-e2e-tests
    
    if [ $? -eq 0 ]; then
        print_success "All tests passed!"
    else
        print_error "Some tests failed"
        exit 1
    fi
}

# Function to show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "OPTIONS:"
    echo "  --install-deps    Install system dependencies"
    echo "  --build           Build the tests"
    echo "  --run             Run the tests"
    echo "  --all             Install dependencies, build, and run tests"
    echo "  --help            Show this help message"
    echo
    echo "Examples:"
    echo "  $0 --all                # Full setup and run"
    echo "  $0 --install-deps       # Only install dependencies"
    echo "  $0 --build              # Only build tests"
    echo "  $0 --run               # Only run tests"
}

# Main script logic
main() {
    local install_deps=false
    local build=false
    local run=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --install-deps)
                install_deps=true
                shift
                ;;
            --build)
                build=true
                shift
                ;;
            --run)
                run=true
                shift
                ;;
            --all)
                install_deps=true
                build=true
                run=true
                shift
                ;;
            --help)
                show_usage
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # If no arguments provided, show usage
    if [ "$install_deps" = false ] && [ "$build" = false ] && [ "$run" = false ]; then
        show_usage
        exit 1
    fi
    
    # Detect OS
    local os=$(detect_os)
    print_status "Detected OS: $os"
    
    # Install dependencies if requested
    if [ "$install_deps" = true ]; then
        case $os in
            linux)
                install_linux_deps
                ;;
            macos)
                install_macos_deps
                ;;
            windows)
                print_error "Windows automatic dependency installation not supported yet"
                print_error "Please install dependencies manually using the README instructions"
                exit 1
                ;;
            *)
                print_error "Unsupported OS: $os"
                exit 1
                ;;
        esac
    fi
    
    # Check dependencies
    if ! check_dependencies; then
        print_error "Dependencies check failed. Please install missing dependencies."
        exit 1
    fi
    
    # Build tests if requested
    if [ "$build" = true ]; then
        if ! build_tests; then
            print_error "Build failed"
            exit 1
        fi
    fi
    
    # Run tests if requested
    if [ "$run" = true ]; then
        # Change to build directory if not already there
        if [ ! -f "video-simili-duplicate-cleaner-e2e-tests" ]; then
            if [ -f "build/video-simili-duplicate-cleaner-e2e-tests" ]; then
                cd build
            else
                print_error "Test executable not found. Please build first."
                exit 1
            fi
        fi
        
        run_tests
    fi
    
    print_success "Script completed successfully!"
}

# Run main function with all arguments
main "$@"