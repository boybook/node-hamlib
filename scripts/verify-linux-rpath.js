#!/usr/bin/env node
/**
 * Verify that Linux prebuilt binaries have correct RUNPATH
 *
 * This script checks that Linux .node files have RUNPATH set to $ORIGIN
 * It's used in CI to verify the fix-linux-rpath.js script worked correctly.
 *
 * Usage: node scripts/verify-linux-rpath.js
 *
 * Exit codes:
 * - 0: All Linux prebuilds have correct RUNPATH
 * - 1: Verification failed or incorrect RUNPATH found
 */

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

function log(msg) {
  console.log(`[verify-rpath] ${msg}`);
}

function error(msg) {
  console.error(`[verify-rpath] ❌ ${msg}`);
}

function success(msg) {
  console.log(`[verify-rpath] ✅ ${msg}`);
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
 * Check if readelf is available
 */
function checkReadelf() {
  try {
    const result = spawnSync('readelf', ['--version'], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });
    return result.status === 0;
  } catch (e) {
    return false;
  }
}

/**
 * Get RUNPATH from a Linux binary using readelf
 */
function getRunpath(binaryPath) {
  try {
    const result = spawnSync('readelf', ['-d', binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status !== 0) {
      error(`readelf failed for ${binaryPath}`);
      return null;
    }

    // Look for RUNPATH line
    const lines = result.stdout.split('\n');
    for (const line of lines) {
      if (line.includes('(RUNPATH)') || line.includes('(RPATH)')) {
        // Extract the path from: 0x000000000000001d (RUNPATH)  Library runpath: [$ORIGIN]
        const match = line.match(/Library (?:runpath|rpath):\s*\[([^\]]*)\]/);
        if (match) {
          return match[1];
        }
      }
    }

    return ''; // No RUNPATH found
  } catch (e) {
    error(`Failed to read RUNPATH from ${binaryPath}: ${e.message}`);
    return null;
  }
}

/**
 * Verify dependencies of a binary using ldd
 * @param {string} binaryPath - Path to the binary
 * @returns {Object} { success: boolean, missing: string[] }
 */
function verifyDependencies(binaryPath) {
  try {
    const result = spawnSync('ldd', [binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status !== 0) {
      error(`ldd failed for ${binaryPath}`);
      return { success: false, missing: [] };
    }

    const output = result.stdout;
    const missing = [];

    for (const line of output.split('\n')) {
      if (line.includes('not found')) {
        // Extract the library name from lines like: "libusb-1.0.so.0 => not found"
        const match = line.match(/^\s*(\S+)\s+=>\s+not found/);
        if (match) {
          missing.push(match[1]);
        }
      }
    }

    return { success: missing.length === 0, missing };
  } catch (e) {
    error(`Failed to check dependencies for ${binaryPath}: ${e.message}`);
    return { success: false, missing: [] };
  }
}

/**
 * Find all Linux prebuilt .node and .so files
 */
function findLinuxPrebuilds(prebuildsDir) {
  const results = [];

  if (!exists(prebuildsDir)) {
    return results;
  }

  try {
    const entries = fs.readdirSync(prebuildsDir, { withFileTypes: true });

    for (const entry of entries) {
      if (!entry.isDirectory()) continue;

      const dirName = entry.name;
      // Match linux-x64, linux-arm64, linux-x64+glibc, etc.
      if (!dirName.startsWith('linux-')) continue;

      const dirPath = path.join(prebuildsDir, dirName);
      const files = fs.readdirSync(dirPath);

      for (const file of files) {
        // Check both .node and .so files (including versioned .so.X)
        if (file.endsWith('.node') || file.endsWith('.so') || /\.so\.\d+$/.test(file)) {
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
 * Main verification function
 */
function main() {
  log('Starting RUNPATH verification...\n');

  // Skip on non-Linux platforms
  if (process.platform !== 'linux') {
    log(`Skipping: Current platform is ${process.platform}, not Linux`);
    log('RUNPATH verification only applies to Linux binaries');
    return 0;
  }

  // Check if readelf is available
  if (!checkReadelf()) {
    error('readelf is not available');
    error('Cannot verify RUNPATH without readelf');
    return 1;
  }

  const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
  const linuxPrebuilds = findLinuxPrebuilds(prebuildsDir);

  if (linuxPrebuilds.length === 0) {
    error('No Linux prebuilt binaries found');
    log('Make sure to run "npm run prebuild:bundle" first');
    return 1;
  }

  log(`Found ${linuxPrebuilds.length} Linux binary(ies) to verify (.node and .so files)\n`);

  const expectedRpath = '$ORIGIN';
  let allCorrect = true;

  for (const prebuild of linuxPrebuilds) {
    log(`📦 Verifying: ${prebuild.platform}/${prebuild.filename}`);

    // Check 1: RUNPATH verification
    const actualRpath = getRunpath(prebuild.path);

    if (actualRpath === null) {
      error(`  RUNPATH check: Failed to read`);
      allCorrect = false;
    } else {
      log(`  RUNPATH: ${actualRpath ? `'${actualRpath}'` : '(empty)'}`);

      if (actualRpath === expectedRpath) {
        success(`  RUNPATH check: Correct!`);
      } else {
        error(`  RUNPATH check: Incorrect! Expected '${expectedRpath}', got '${actualRpath}'`);
        allCorrect = false;
      }
    }

    // Check 2: Dependency verification using ldd
    log(`  Checking dependencies with ldd...`);
    const depCheck = verifyDependencies(prebuild.path);

    if (depCheck.success) {
      success(`  Dependencies: All resolved ✓`);
    } else {
      error(`  Dependencies: Missing ${depCheck.missing.length} library(ies):`);
      for (const missing of depCheck.missing) {
        error(`    - ${missing}`);
      }
      allCorrect = false;
    }

    console.log('');
  }

  // Summary
  log('='.repeat(50));
  if (allCorrect) {
    success('All Linux prebuilt binaries passed verification!');
    success('- RUNPATH is correctly set to $ORIGIN');
    success('- All dependencies are resolved');
    log('='.repeat(50));
    return 0;
  } else {
    error('Some binaries failed verification:');
    error('- Check RUNPATH issues above');
    error('- Check missing dependency issues above');
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

module.exports = { main, getRunpath };
