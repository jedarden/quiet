#!/bin/bash

# Script to test GitHub Actions workflows locally using act
# https://github.com/nektos/act

set -e

echo "========================================="
echo "GitHub Actions Local Test Runner"
echo "========================================="

# Check if act is installed
if ! command -v act &> /dev/null; then
    echo "act is not installed. Install it first:"
    echo ""
    echo "# macOS:"
    echo "brew install act"
    echo ""
    echo "# Linux:"
    echo "curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash"
    echo ""
    echo "# Or download from: https://github.com/nektos/act/releases"
    exit 1
fi

# Check if Docker is running
if ! docker info &> /dev/null; then
    echo "Error: Docker is not running. Please start Docker first."
    exit 1
fi

echo "Found act version: $(act --version)"

# Function to run a specific job
run_job() {
    local job_name=$1
    local workflow_file=${2:-".github/workflows/release.yml"}
    
    echo ""
    echo "Running job: $job_name from $workflow_file"
    echo "----------------------------------------"
    
    # Create .actrc file for configuration
    cat > .actrc << EOF
# Use larger runner images for better compatibility
-P ubuntu-latest=catthehacker/ubuntu:full-latest
-P windows-latest=catthehacker/ubuntu:full-latest
-P macos-latest=catthehacker/ubuntu:full-latest
EOF
    
    # Run the job
    act -j $job_name \
        --workflow $workflow_file \
        --platform ubuntu-latest=catthehacker/ubuntu:full-latest \
        --platform windows-latest=catthehacker/ubuntu:full-latest \
        --platform macos-latest=catthehacker/ubuntu:full-latest \
        --verbose
}

# Menu
echo ""
echo "Select what to test:"
echo "1) Test Windows build"
echo "2) Test macOS build"
echo "3) Test release creation"
echo "4) Test full workflow"
echo "5) List all jobs"
echo "6) Dry run (show what would be executed)"
echo "7) Test with custom workflow file"

read -p "Enter choice (1-7): " choice

case $choice in
    1)
        run_job "build-windows"
        ;;
    2)
        run_job "build-macos"
        ;;
    3)
        run_job "create-release"
        ;;
    4)
        echo "Running full workflow..."
        act push --tag v1.0.0
        ;;
    5)
        echo "Available jobs:"
        act -l
        ;;
    6)
        echo "Dry run - showing what would be executed:"
        act -n push --tag v1.0.0
        ;;
    7)
        read -p "Enter workflow file path: " workflow_file
        read -p "Enter job name: " job_name
        run_job "$job_name" "$workflow_file"
        ;;
    *)
        echo "Invalid choice"
        exit 1
        ;;
esac

echo ""
echo "========================================="
echo "Test completed"
echo "========================================="

# Cleanup
rm -f .actrc

# Tips
echo ""
echo "Tips for debugging:"
echo "- Use 'act -v' for verbose output"
echo "- Use 'act --container-architecture linux/amd64' if on Apple Silicon"
echo "- Check ~/.cache/act for downloaded images"
echo "- Use 'act --rm' to remove containers after run"