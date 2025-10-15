#!/usr/bin/env node
/**
 * Fix Linux prebuilt binaries RUNPATH to use $ORIGIN
 *
 * This script uses patchelf to set the correct RUNPATH for Linux prebuilt binaries.
 * It fixes the issue where node-gyp/make mangles $ORIGIN in binding.gyp ldflags.
 *
 * Usage: node scripts/fix-linux-rpath.js
 *
 * Requirements:
 * - patchelf must be installed on Linux systems
 * - Only runs on Linux platform (skips on macOS/Windows)
 */

const fs = require('fs');
const path = require('path');
const { execSync, spawnSync } = require('child_process');

function log(msg) {
  console.log(`[fix-linux-rpath] ${msg}`);
}

function warn(msg) {
  console.warn(`[fix-linux-rpath] ⚠️  ${msg}`);
}

function error(msg) {
  console.error(`[fix-linux-rpath] ❌ ${msg}`);
}

function success(msg) {
  console.log(`[fix-linux-rpath] ✅ ${msg}`);
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
 * Check if patchelf is available
 */
function checkPatchelf() {
  try {
    const result = spawnSync('patchelf', ['--version'], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      log(`patchelf found: ${result.stdout.trim()}`);
      return true;
    }
  } catch (e) {
    // patchelf not found
  }
  return false;
}

/**
 * Get current RUNPATH of a binary
 */
function getCurrentRunpath(binaryPath) {
  try {
    const result = spawnSync('patchelf', ['--print-rpath', binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      return result.stdout.trim();
    }
  } catch (e) {
    warn(`Failed to read RUNPATH from ${binaryPath}: ${e.message}`);
  }
  return null;
}

/**
 * Set RUNPATH for a binary using patchelf
 */
function setRunpath(binaryPath, rpath) {
  try {
    log(`Setting RUNPATH to '${rpath}' for ${binaryPath}`);

    const result = spawnSync('patchelf', ['--set-rpath', rpath, binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      success(`Successfully set RUNPATH for ${path.basename(binaryPath)}`);
      return true;
    } else {
      error(`patchelf failed: ${result.stderr}`);
      return false;
    }
  } catch (e) {
    error(`Failed to set RUNPATH: ${e.message}`);
    return false;
  }
}

/**
 * Verify RUNPATH is correctly set
 */
function verifyRunpath(binaryPath, expectedRpath) {
  const actualRpath = getCurrentRunpath(binaryPath);

  if (actualRpath === expectedRpath) {
    success(`Verified RUNPATH is correctly set to '${actualRpath}'`);
    return true;
  } else {
    warn(`RUNPATH verification failed. Expected: '${expectedRpath}', Got: '${actualRpath}'`);
    return false;
  }
}

/**
 * Find all .node files in prebuilds directory that need fixing
 */
function findLinuxPrebuilds(prebuildsDir) {
  const results = [];

  if (!exists(prebuildsDir)) {
    warn(`Prebuilds directory not found: ${prebuildsDir}`);
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
        if (file.endsWith('.node')) {
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
 * Main function
 */
function main() {
  log('Starting Linux RUNPATH fix...');

  // Skip on non-Linux platforms
  if (process.platform !== 'linux') {
    log(`Skipping: Current platform is ${process.platform}, not Linux`);
    log('This script only runs on Linux to fix Linux prebuilt binaries');
    return 0;
  }

  // Check if patchelf is available
  if (!checkPatchelf()) {
    error('patchelf is not installed or not in PATH');
    error('Please install patchelf: sudo apt-get install patchelf');
    return 1;
  }

  const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
  const linuxPrebuilds = findLinuxPrebuilds(prebuildsDir);

  if (linuxPrebuilds.length === 0) {
    warn('No Linux prebuilt binaries found');
    log('Make sure to run "npm run prebuild" first');
    return 0;
  }

  log(`Found ${linuxPrebuilds.length} Linux prebuild(s) to fix`);

  let successCount = 0;
  let failCount = 0;

  for (const prebuild of linuxPrebuilds) {
    log(`\n📦 Processing: ${prebuild.platform}/${prebuild.filename}`);

    // Check current RUNPATH
    const currentRpath = getCurrentRunpath(prebuild.path);
    log(`  Current RUNPATH: ${currentRpath ? `'${currentRpath}'` : '(empty)'}`);

    // Set RUNPATH to $ORIGIN
    const targetRpath = '$ORIGIN';

    if (currentRpath === targetRpath) {
      success(`  RUNPATH already correct, skipping`);
      successCount++;
      continue;
    }

    // Apply fix
    if (setRunpath(prebuild.path, targetRpath)) {
      // Verify the fix
      if (verifyRunpath(prebuild.path, targetRpath)) {
        successCount++;
      } else {
        failCount++;
      }
    } else {
      failCount++;
    }
  }

  // Summary
  log('\n' + '='.repeat(50));
  log('Summary:');
  log(`  Total binaries: ${linuxPrebuilds.length}`);
  log(`  ✅ Successfully fixed: ${successCount}`);
  if (failCount > 0) {
    log(`  ❌ Failed: ${failCount}`);
  }
  log('='.repeat(50));

  if (failCount > 0) {
    error('Some binaries failed to fix. Please check the logs above.');
    return 1;
  }

  success('All Linux prebuilt binaries have been fixed!');
  return 0;
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

module.exports = { main, setRunpath, verifyRunpath };
