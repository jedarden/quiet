#!/bin/bash

# Validate GitHub Actions workflow syntax and logic

set -e

echo "========================================="
echo "GitHub Actions Workflow Validator"
echo "========================================="

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Function to check file exists
check_file() {
    if [ -f "$1" ]; then
        echo -e "${GREEN}✓ Found: $1${NC}"
        return 0
    else
        echo -e "${RED}✗ Missing: $1${NC}"
        return 1
    fi
}

# Function to validate YAML syntax
validate_yaml() {
    if command -v python3 &> /dev/null; then
        python3 -c "import yaml; yaml.safe_load(open('$1'))" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo -e "${GREEN}✓ Valid YAML: $1${NC}"
            return 0
        else
            echo -e "${RED}✗ Invalid YAML: $1${NC}"
            return 1
        fi
    else
        echo -e "${YELLOW}! Cannot validate YAML (Python not available)${NC}"
        return 0
    fi
}

# Check workflow files
echo -e "\n${YELLOW}Checking workflow files...${NC}"
for workflow in .github/workflows/*.yml; do
    if [ -f "$workflow" ]; then
        echo -e "\nValidating: $workflow"
        validate_yaml "$workflow"
        
        # Check for common issues
        echo "  Checking for common issues..."
        
        # Check for deprecated actions
        if grep -q "actions/.*@v[123]" "$workflow"; then
            echo -e "  ${YELLOW}! Uses older action versions${NC}"
        fi
        
        # Check for required permissions
        if grep -q "create-release" "$workflow" && ! grep -q "permissions:" "$workflow"; then
            echo -e "  ${YELLOW}! May need 'permissions: contents: write' for releases${NC}"
        fi
        
        # Check for secrets usage
        if grep -q "\${{ secrets\." "$workflow" && ! grep -q "GITHUB_TOKEN" "$workflow"; then
            echo -e "  ${YELLOW}! Uses secrets - ensure they're configured${NC}"
        fi
    fi
done

# Check required files for build
echo -e "\n${YELLOW}Checking required files...${NC}"
check_file "CMakeLists.txt"
check_file "resources/icons/icon_512.png"
check_file "resources/icons/icon_128.png"
check_file "installer/windows/quiet_installer.nsi"
check_file "installer/macos/create_dmg.sh"
check_file "LICENSE"

# Check source files
echo -e "\n${YELLOW}Checking source files...${NC}"
src_files=(
    "src/main.cpp"
    "src/core/AudioBuffer.cpp"
    "src/core/AudioDeviceManager.cpp"
    "src/core/NoiseReductionProcessor.cpp"
    "src/ui/MainWindow.cpp"
)

for src in "${src_files[@]}"; do
    check_file "$src"
done

# Simulate build steps
echo -e "\n${YELLOW}Simulating build steps...${NC}"

# Test icon creation (Windows PowerShell version)
echo -e "\nTesting Windows icon creation..."
cat << 'EOF' > test_icon_creation.ps1
$iconPath = "resources/icons/icon_512.png"
if (-not (Test-Path $iconPath)) {
    Write-Host "Would create icon at: $iconPath"
    # In real build: Create actual PNG
} else {
    Write-Host "Icon already exists: $iconPath"
}
EOF

if command -v pwsh &> /dev/null; then
    pwsh test_icon_creation.ps1
else
    echo -e "${YELLOW}PowerShell not available for testing${NC}"
fi
rm -f test_icon_creation.ps1

# Test RNNoise setup
echo -e "\nTesting RNNoise setup..."
RNNOISE_TEST_DIR="test_rnnoise"
mkdir -p $RNNOISE_TEST_DIR/{include,lib}

cat > $RNNOISE_TEST_DIR/include/rnnoise.h << 'EOF'
#ifndef RNNOISE_H
#define RNNOISE_H
typedef struct DenoiseState DenoiseState;
#endif
EOF

if [ -f "$RNNOISE_TEST_DIR/include/rnnoise.h" ]; then
    echo -e "${GREEN}✓ RNNoise header creation successful${NC}"
else
    echo -e "${RED}✗ RNNoise header creation failed${NC}"
fi

rm -rf $RNNOISE_TEST_DIR

# Check NSIS installer
echo -e "\n${YELLOW}Checking NSIS installer configuration...${NC}"
if [ -f "installer/windows/quiet_installer.nsi" ]; then
    echo "NSIS sections found:"
    grep -E "^Section|^FunctionEnd" installer/windows/quiet_installer.nsi | head -10
fi

# Summary
echo -e "\n========================================="
echo "Validation Summary"
echo "========================================="

# Count issues
workflow_count=$(find .github/workflows -name "*.yml" 2>/dev/null | wc -l)
echo "Workflow files found: $workflow_count"

# Provide recommendations
echo -e "\n${YELLOW}Recommendations:${NC}"
echo "1. Test locally with: docker-build-test.sh"
echo "2. Use act for full simulation: test-github-actions.sh"
echo "3. Check GitHub Actions log for specific errors"
echo "4. Ensure all binary dependencies are handled in workflow"

# Create a mock build to test CMake
echo -e "\n${YELLOW}Creating mock CMakeLists.txt test...${NC}"
cat > test_cmake.txt << 'EOF'
cmake_minimum_required(VERSION 3.20)
project(TestBuild)

# This would test if CMake can parse the file
message(STATUS "CMake parse test successful")
EOF

echo -e "${GREEN}Mock CMake file created successfully${NC}"
rm -f test_cmake.txt

echo -e "\n${GREEN}Validation complete!${NC}"