#!/bin/bash

# Debug GitHub Actions workflow issues

echo "========================================="
echo "GitHub Actions Workflow Debugger"
echo "========================================="

# Check workflow file
WORKFLOW=".github/workflows/release.yml"

if [ ! -f "$WORKFLOW" ]; then
    echo "ERROR: Workflow file not found: $WORKFLOW"
    exit 1
fi

echo "Analyzing workflow: $WORKFLOW"
echo ""

# Extract key information
echo "1. Checking triggers:"
grep -A5 "^on:" "$WORKFLOW" | head -10

echo -e "\n2. Checking jobs:"
grep -E "^[[:space:]]+[a-zA-Z-]+:" "$WORKFLOW" | grep -v "steps:" | head -10

echo -e "\n3. Checking for deprecated actions:"
grep -n "@v[123]" "$WORKFLOW" || echo "✓ No deprecated v1/v2/v3 actions found"

echo -e "\n4. Checking permissions:"
grep -B2 -A2 "permissions:" "$WORKFLOW" || echo "! No explicit permissions found"

echo -e "\n5. Checking for icon references:"
grep -n "icon" "$WORKFLOW" | head -5

echo -e "\n6. Checking RNNoise references:"
grep -n -i "rnnoise" "$WORKFLOW" | head -5

echo -e "\n7. Common issues to check:"
echo "   - Icon files must exist: resources/icons/icon_512.png and icon_128.png"
echo "   - RNNoise library must be available or built"
echo "   - CMake must be able to find all dependencies"
echo "   - NSIS must be installed for Windows builds"

echo -e "\n8. Testing local file structure:"
echo "   Checking required files..."

# Check required files
files_to_check=(
    "CMakeLists.txt"
    "resources/icons/icon_512.png"
    "resources/icons/icon_128.png"
    "src/main.cpp"
    "installer/windows/quiet_installer.nsi"
    "installer/macos/create_dmg.sh"
)

missing_files=0
for file in "${files_to_check[@]}"; do
    if [ -f "$file" ]; then
        echo "   ✓ $file"
    else
        echo "   ✗ $file (MISSING)"
        ((missing_files++))
    fi
done

echo -e "\n9. Quick fixes:"
if [ $missing_files -gt 0 ]; then
    echo "   Create missing files:"
    echo "   mkdir -p resources/icons"
    echo "   touch resources/icons/icon_512.png resources/icons/icon_128.png"
fi

echo -e "\n10. Simulating build environment setup:"
echo "    Would create:"
echo "    - build/rnnoise/include/rnnoise.h"
echo "    - build/rnnoise/lib/librnnoise.a (or .lib on Windows)"

echo -e "\n========================================="
echo "Debugging complete"
echo "========================================="

echo -e "\nTo test the build locally without GitHub Actions:"
echo "1. Use Docker: ./docker-build-test.sh"
echo "2. Create a minimal test build:"
echo "   mkdir -p build && cd build"
echo "   touch dummy_cmake_test"
echo "3. Or use the GitHub CLI to trigger a workflow run:"
echo "   gh workflow run release.yml"