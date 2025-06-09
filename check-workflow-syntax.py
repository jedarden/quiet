#!/usr/bin/env python3

import yaml
import sys
import os
from pathlib import Path

def check_workflow(filepath):
    """Check GitHub Actions workflow for common issues"""
    print(f"\nChecking: {filepath}")
    
    try:
        with open(filepath, 'r') as f:
            workflow = yaml.safe_load(f)
        print("✓ Valid YAML syntax")
        
        # Check for common issues
        issues = []
        warnings = []
        
        # Check jobs
        if 'jobs' not in workflow:
            issues.append("No jobs defined")
        else:
            for job_name, job in workflow['jobs'].items():
                print(f"  Job: {job_name}")
                
                # Check for steps
                if 'steps' not in job:
                    issues.append(f"Job '{job_name}' has no steps")
                else:
                    for i, step in enumerate(job['steps']):
                        # Check for uses with old versions
                        if 'uses' in step:
                            action = step['uses']
                            if '@v3' in action or '@v2' in action or '@v1' in action:
                                warnings.append(f"Step {i} in job '{job_name}' uses old action version: {action}")
                        
                        # Check for required fields
                        if 'name' not in step and 'uses' not in step and 'run' not in step:
                            issues.append(f"Step {i} in job '{job_name}' has no name, uses, or run")
                
                # Check permissions for release job
                if 'release' in job_name.lower() and 'permissions' not in job:
                    warnings.append(f"Job '{job_name}' may need 'permissions: contents: write'")
        
        # Check triggers
        if 'on' not in workflow:
            issues.append("No triggers defined")
        
        # Report results
        if issues:
            print("\n❌ Issues found:")
            for issue in issues:
                print(f"  - {issue}")
        
        if warnings:
            print("\n⚠️  Warnings:")
            for warning in warnings:
                print(f"  - {warning}")
        
        if not issues and not warnings:
            print("✓ No issues found")
            
        return len(issues) == 0
        
    except yaml.YAMLError as e:
        print(f"❌ YAML parsing error: {e}")
        return False
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

def main():
    workflows_dir = Path(".github/workflows")
    
    if not workflows_dir.exists():
        print("No .github/workflows directory found")
        return 1
    
    all_valid = True
    
    for workflow_file in workflows_dir.glob("*.yml"):
        if not check_workflow(workflow_file):
            all_valid = False
    
    for workflow_file in workflows_dir.glob("*.yaml"):
        if not check_workflow(workflow_file):
            all_valid = False
    
    print("\n" + "="*40)
    if all_valid:
        print("✅ All workflows are valid")
        return 0
    else:
        print("❌ Some workflows have issues")
        return 1

if __name__ == "__main__":
    sys.exit(main())