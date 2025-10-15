#!/usr/bin/env node
/**
 * Unified dependency verification script for all platforms
 *
 * This script verifies that prebuilt binaries have all required dependencies:
 * - Linux: Checks RUNPATH and ldd dependencies
 * - macOS: Checks install_name and otool dependencies
 * - Windows: Checks for required DLLs in the same directory
 *
 * Usage: node scripts/verify-dependencies.js
 *
 * Exit codes:
 * - 0: All binaries passed verification
 * - 1: Verification failed or missing dependencies found
 */

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

function log(msg) {
  console.log(`[verify-deps] ${msg}`);
}

function error(msg) {
  console.error(`[verify-deps] ❌ ${msg}`);
}

function success(msg) {
  console.log(`[verify-deps] ✅ ${msg}`);
}

function exists(filePath) {
  try {
    fs.accessSync(filePath);
    return true;
  } catch {
    return false;
  }
}

/**
 * Find all prebuilt binaries for the current platform
 */
function findPrebuilds(prebuildsDir) {
  const results = [];
  const platform = process.platform; // 'linux' | 'darwin' | 'win32'

  if (!exists(prebuildsDir)) {
    return results;
  }

  try {
    const entries = fs.readdirSync(prebuildsDir, { withFileTypes: true });

    for (const entry of entries) {
      if (!entry.isDirectory()) continue;

      const dirName = entry.name;

      // Match platform-specific directories
      let shouldProcess = false;
      if (platform === 'linux' && dirName.startsWith('linux-')) shouldProcess = true;
      if (platform === 'darwin' && dirName.startsWith('darwin-')) shouldProcess = true;
      if (platform === 'win32' && dirName.startsWith('win32-')) shouldProcess = true;

      if (!shouldProcess) continue;

      const dirPath = path.join(prebuildsDir, dirName);
      const files = fs.readdirSync(dirPath);

      for (const file of files) {
        // Platform-specific file types
        let isRelevant = false;
        if (platform === 'linux' && (file.endsWith('.node') || file.endsWith('.so') || /\.so\.\d+$/.test(file))) {
          isRelevant = true;
        } else if (platform === 'darwin' && (file.endsWith('.node') || file.endsWith('.dylib'))) {
          isRelevant = true;
        } else if (platform === 'win32' && (file.endsWith('.node') || file.endsWith('.dll'))) {
          isRelevant = true;
        }

        if (isRelevant) {
          const filePath = path.join(dirPath, file);
          results.push({
            path: filePath,
            platform: dirName,
            filename: file
          });
        }
      }
    }
  } catch (e) {
    error(`Failed to scan prebuilds directory: ${e.message}`);
  }

  return results;
}

/**
 * Verify Linux binaries (RUNPATH + ldd)
 */
function verifyLinux(binaryPath) {
  const errors = [];

  // Check 1: RUNPATH
  try {
    const result = spawnSync('readelf', ['-d', binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      const lines = result.stdout.split('\n');
      let runpath = '';
      for (const line of lines) {
        if (line.includes('(RUNPATH)') || line.includes('(RPATH)')) {
          const match = line.match(/Library (?:runpath|rpath):\s*\[([^\]]*)\]/);
          if (match) {
            runpath = match[1];
          }
        }
      }

      if (runpath !== '$ORIGIN') {
        errors.push(`RUNPATH is '${runpath}', expected '$ORIGIN'`);
      }
    } else {
      errors.push('Failed to read RUNPATH');
    }
  } catch (e) {
    errors.push(`readelf not available: ${e.message}`);
  }

  // Check 2: ldd dependencies
  try {
    const result = spawnSync('ldd', [binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      const output = result.stdout;
      const missing = [];

      for (const line of output.split('\n')) {
        if (line.includes('not found')) {
          const match = line.match(/^\s*(\S+)\s+=>\s+not found/);
          if (match) {
            missing.push(match[1]);
          }
        }
      }

      if (missing.length > 0) {
        errors.push(`Missing dependencies: ${missing.join(', ')}`);
      }
    }
  } catch (e) {
    errors.push(`ldd check failed: ${e.message}`);
  }

  return errors;
}

/**
 * Verify macOS binaries (install_name + otool)
 */
function verifyMac(binaryPath) {
  const errors = [];

  try {
    const result = spawnSync('otool', ['-L', binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      const output = result.stdout;
      const lines = output.split('\n');

      for (const line of lines) {
        const match = line.match(/^\s+(\S+\.dylib)\s+\(/);
        if (!match) continue;

        const dylibPath = match[1];

        // Check if it's using absolute paths that point outside /usr/lib or /System
        if (dylibPath.startsWith('/') &&
            !dylibPath.startsWith('/usr/lib/') &&
            !dylibPath.startsWith('/System/Library/') &&
            !dylibPath.startsWith('@loader_path/') &&
            !dylibPath.startsWith('@rpath/')) {

          // This is a third-party library - check if it exists in the same directory
          const dylibName = path.basename(dylibPath);
          const localPath = path.join(path.dirname(binaryPath), dylibName);

          if (!exists(localPath) && !exists(dylibPath)) {
            errors.push(`Missing dependency: ${dylibName} (referenced as ${dylibPath})`);
          }
        }
      }
    } else {
      errors.push('Failed to read dependencies with otool');
    }
  } catch (e) {
    errors.push(`otool not available: ${e.message}`);
  }

  return errors;
}

/**
 * Verify Windows binaries (check for DLLs in same directory)
 */
function verifyWindows(binaryPath) {
  const errors = [];

  // For Windows, we mainly check that common dependencies are present
  // since Windows DLL resolution is more flexible (checks same directory automatically)

  const binDir = path.dirname(binaryPath);
  const expectedDlls = [
    // We expect hamlib DLL to be present
    /^(lib)?hamlib(-\d+)?\.dll$/i,
  ];

  const files = fs.readdirSync(binDir);
  const foundDlls = files.filter(f => f.endsWith('.dll'));

  // Check if hamlib DLL is present
  const hasHamlib = foundDlls.some(dll => expectedDlls.some(pattern => pattern.test(dll)));

  if (!hasHamlib) {
    errors.push('Hamlib DLL not found in prebuilds directory');
  }

  // Note: We don't strictly check for other DLLs as they may or may not be needed
  // depending on the hamlib build configuration
  return errors;
}

/**
 * Main verification function
 */
function main() {
  const platform = process.platform;
  log(`Starting dependency verification for ${platform}...\n`);

  const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
  const prebuilds = findPrebuilds(prebuildsDir);

  if (prebuilds.length === 0) {
    error(`No prebuilt binaries found for ${platform}`);
    log('Make sure to run "npm run prebuild:bundle" first');
    return 1;
  }

  log(`Found ${prebuilds.length} binary(ies) to verify\n`);

  let allPassed = true;

  for (const prebuild of prebuilds) {
    log(`📦 Verifying: ${prebuild.platform}/${prebuild.filename}`);

    let errors = [];

    if (platform === 'linux') {
      errors = verifyLinux(prebuild.path);
    } else if (platform === 'darwin') {
      errors = verifyMac(prebuild.path);
    } else if (platform === 'win32') {
      errors = verifyWindows(prebuild.path);
    }

    if (errors.length === 0) {
      success('  All checks passed');
    } else {
      allPassed = false;
      for (const err of errors) {
        error(`  ${err}`);
      }
    }

    console.log('');
  }

  // Summary
  log('='.repeat(50));
  if (allPassed) {
    success('All binaries passed verification!');
    success(`Platform: ${platform}`);
    log('='.repeat(50));
    return 0;
  } else {
    error('Some binaries failed verification');
    error('Check the error messages above');
    log('='.repeat(50));
    return 1;
  }
}

// Run main function
if (require.main === module) {
  try {
    const exitCode = main();
    process.exit(exitCode);
  } catch (e) {
    error(`Unexpected error: ${e.message}`);
    console.error(e.stack);
    process.exit(1);
  }
}

module.exports = { main };
