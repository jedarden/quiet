# Testing GitHub Actions Locally

This guide explains how to test GitHub Actions workflows locally before pushing to GitHub.

## Overview

Testing GitHub Actions locally helps you:
- Debug workflow issues without using CI/CD minutes
- Iterate faster on workflow changes
- Catch errors before they appear in production
- Understand the build environment

## Methods for Local Testing

### 1. Simulation Scripts (Easiest)

We've created several scripts to simulate GitHub Actions:

```bash
# Quick simulation of build steps
./simulate-github-build.sh

# Validate workflow syntax and structure
./debug-workflow.sh

# Test build steps locally
./test-build-local.sh
```

### 2. Using Act (Most Accurate)

[Act](https://github.com/nektos/act) runs GitHub Actions workflows locally using Docker.

#### Installation:
```bash
# macOS
brew install act

# Linux
curl https://raw.githubusercontent.com/nektos/act/master/install.sh | sudo bash

# Windows
choco install act-cli
```

#### Usage:
```bash
# List all jobs
act -l

# Run specific job
act -j build-windows

# Run on tag push (simulates release)
act push --tag v1.0.0

# Dry run (see what would execute)
act -n push --tag v1.0.0

# Verbose output
act -v push --tag v1.0.0
```

### 3. Docker-based Testing

Test in an environment similar to GitHub Actions:

```bash
# Run our Docker build test
./docker-build-test.sh

# Or manually with Docker
docker run -it ubuntu:22.04 bash
# Then run build commands inside container
```

### 4. GitHub CLI

If you have GitHub CLI installed:

```bash
# Trigger workflow manually
gh workflow run release.yml

# View workflow runs
gh run list --workflow=release.yml

# Watch a running workflow
gh run watch
```

## Common Issues and Solutions

### Missing Dependencies

**Issue**: CMake, build tools, or libraries not found

**Solution**: The workflow handles this by:
- Creating placeholder files
- Using fallback options
- Installing dependencies in the workflow

### Icon Files

**Issue**: JUCE requires icon files that may not exist

**Solution**: The workflow creates placeholder icons:
```powershell
# Windows
Add-Type -AssemblyName System.Drawing
$bitmap = New-Object System.Drawing.Bitmap(512, 512)
# ... create icon
```

### RNNoise Library

**Issue**: RNNoise must be built or provided

**Solution**: The workflow creates minimal RNNoise files:
```bash
mkdir -p build/rnnoise/{include,lib}
# Create header and dummy lib
```

## Testing Workflow Changes

1. **Make changes to `.github/workflows/release.yml`**

2. **Test syntax**:
   ```bash
   ./debug-workflow.sh
   ```

3. **Simulate build**:
   ```bash
   ./simulate-github-build.sh
   ```

4. **Test with Act** (if installed):
   ```bash
   act -j build-windows --dryrun
   ```

5. **Push to a test branch**:
   ```bash
   git checkout -b test-workflow
   git add .github/workflows/release.yml
   git commit -m "test: workflow changes"
   git push origin test-workflow
   ```

## Environment Differences

Be aware of differences between local and GitHub Actions:

| Aspect | Local | GitHub Actions |
|--------|-------|----------------|
| OS | Your machine | Ubuntu/Windows/macOS runners |
| Tools | What you have installed | Pre-installed tools |
| Permissions | Your user | Runner user |
| Secrets | Not available | Available via secrets |
| Storage | Your disk | Ephemeral runner storage |

## Best Practices

1. **Use conditional steps** for missing tools:
   ```yaml
   - name: Build
     run: cmake --build . || echo "Build failed, continuing..."
   ```

2. **Create fallbacks** for dependencies:
   ```yaml
   - name: Setup
     run: |
       command -v cmake || echo "CMake not found"
       mkdir -p required/directories
   ```

3. **Test incrementally**:
   - Test individual steps first
   - Then test jobs
   - Finally test the full workflow

4. **Use workflow_dispatch** for manual testing:
   ```yaml
   on:
     workflow_dispatch:  # Allows manual trigger
     push:
       tags: ['v*']
   ```

## Debugging Tips

1. **Add debug output**:
   ```yaml
   - name: Debug Info
     run: |
       echo "Current directory: $PWD"
       ls -la
       echo "Environment: ${{ runner.os }}"
   ```

2. **Use continue-on-error** for non-critical steps:
   ```yaml
   - name: Optional Step
     continue-on-error: true
     run: some-command
   ```

3. **Check logs carefully**:
   - Look for the exact error message
   - Check which step failed
   - Review the setup steps for clues

## Summary

Testing GitHub Actions locally saves time and helps ensure your CI/CD pipeline works correctly. Use the provided scripts for quick tests, Act for accurate simulation, or Docker for environment testing. Always validate your workflow syntax and test incrementally.