#!/bin/bash

# Script to build and run tests for the file sync project

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Creating build directory...${NC}"
mkdir -p build
cd build

echo -e "${YELLOW}Running CMake...${NC}"
cmake ..

echo -e "${YELLOW}Building project...${NC}"
cmake --build . -- -j$(nproc)

echo -e "${YELLOW}Running tests...${NC}"
if ctest --output-on-failure; then
    echo -e "${GREEN}All tests passed!${NC}"
else
    echo -e "${RED}Some tests failed!${NC}"
    exit 1
fi