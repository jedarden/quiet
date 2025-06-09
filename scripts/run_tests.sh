#!/bin/bash

# QUIET Test Runner Script
# Runs all unit, integration, and performance tests

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "==================================="
echo "QUIET - Test Suite Runner"
echo "==================================="

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Build directory not found. Creating and configuring...${NC}"
    mkdir -p build
    cd build
    cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
    cd ..
fi

# Build tests
echo -e "\n${YELLOW}Building tests...${NC}"
cd build
make -j$(nproc)
cd ..

# Run unit tests
echo -e "\n${YELLOW}Running unit tests...${NC}"
if [ -f "build/tests/quiet_unit_tests" ]; then
    ./build/tests/quiet_unit_tests --gtest_output=xml:test_results/unit_tests.xml
    UNIT_RESULT=$?
else
    echo -e "${RED}Unit tests not found!${NC}"
    UNIT_RESULT=1
fi

# Run integration tests
echo -e "\n${YELLOW}Running integration tests...${NC}"
if [ -f "build/tests/quiet_integration_tests" ]; then
    ./build/tests/quiet_integration_tests --gtest_output=xml:test_results/integration_tests.xml
    INTEGRATION_RESULT=$?
else
    echo -e "${RED}Integration tests not found!${NC}"
    INTEGRATION_RESULT=1
fi

# Run performance tests
echo -e "\n${YELLOW}Running performance validation...${NC}"
if [ -f "build/tests/quiet_performance_tests" ]; then
    ./build/tests/quiet_performance_tests --gtest_output=xml:test_results/performance_tests.xml
    PERFORMANCE_RESULT=$?
else
    echo -e "${RED}Performance tests not found!${NC}"
    PERFORMANCE_RESULT=1
fi

# Summary
echo -e "\n==================================="
echo "Test Results Summary"
echo "==================================="

if [ $UNIT_RESULT -eq 0 ]; then
    echo -e "Unit Tests:        ${GREEN}PASSED${NC}"
else
    echo -e "Unit Tests:        ${RED}FAILED${NC}"
fi

if [ $INTEGRATION_RESULT -eq 0 ]; then
    echo -e "Integration Tests: ${GREEN}PASSED${NC}"
else
    echo -e "Integration Tests: ${RED}FAILED${NC}"
fi

if [ $PERFORMANCE_RESULT -eq 0 ]; then
    echo -e "Performance Tests: ${GREEN}PASSED${NC}"
else
    echo -e "Performance Tests: ${RED}FAILED${NC}"
fi

# Check code coverage (if enabled)
if command -v gcov &> /dev/null && [ -d "build/CMakeFiles/quiet.dir" ]; then
    echo -e "\n${YELLOW}Generating code coverage report...${NC}"
    cd build
    gcov CMakeFiles/quiet.dir/src/core/*.gcno
    cd ..
    echo -e "${GREEN}Coverage report generated in build/coverage${NC}"
fi

# Exit with appropriate code
if [ $UNIT_RESULT -eq 0 ] && [ $INTEGRATION_RESULT -eq 0 ] && [ $PERFORMANCE_RESULT -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed!${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed!${NC}"
    exit 1
fi