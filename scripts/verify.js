#!/usr/bin/env node

/**
 * Unified verification script
 * Verifies prebuilt binaries and their dependencies
 *
 * Usage:
 *   node scripts/verify.js             - Verify current platform
 *   node scripts/verify.js --all       - Verify all platforms (CI mode)
 */

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { createLogger } = require('./logger');

const logger = createLogger('Verifying Build');

function exists(p) {
  try {
    fs.accessSync(p);
    return true;
  } catch {
    return false;
  }
}

/**
 * Find prebuilt binaries
 */
function findPrebuilds(prebuildsDir, platformFilter = null) {
  const results = [];

  if (!exists(prebuildsDir)) {
    return results;
  }

  const entries = fs.readdirSync(prebuildsDir, { withFileTypes: true });

  for (const entry of entries) {
    if (!entry.isDirectory()) continue;

    const dirName = entry.name;

    // Filter by platform if specified
    if (platformFilter && !dirName.startsWith(platformFilter)) {
      continue;
    }

    const dirPath = path.join(prebuildsDir, dirName);
    const files = fs.readdirSync(dirPath);

    const binary = files.find(f => f.match(/^node\.napi(\.[^.]+)?\.node$/));
    if (binary) {
      results.push({
        path: path.join(dirPath, binary),
        dir: dirPath,
        platform: dirName,
        filename: binary
      });
    }
  }

  return results;
}

/**
 * Linux verification
 */
function verifyLinux(binaryPath) {
  const errors = [];
  const warnings = [];

  // Check RUNPATH
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
        warnings.push(`RUNPATH is '${runpath}', expected '$ORIGIN'`);
      } else {
        logger.log('RUNPATH=$ORIGIN', 'success');
      }
    }
  } catch (e) {
    warnings.push('readelf not available');
  }

  // Check ldd dependencies
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
      } else {
        logger.log('All dependencies resolved', 'success');
      }
    }
  } catch (e) {
    warnings.push('ldd check failed');
  }

  return { errors, warnings };
}

/**
 * macOS verification
 */
function verifyMac(binaryPath) {
  const errors = [];
  const warnings = [];

  try {
    const result = spawnSync('otool', ['-L', binaryPath], {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });

    if (result.status === 0) {
      const output = result.stdout;
      const lines = output.split('\n');
      let hasLoaderPath = false;

      for (const line of lines) {
        const match = line.match(/^\s+(\S+\.dylib)\s+\(/);
        if (!match) continue;

        const dylibPath = match[1];

        if (dylibPath.includes('@loader_path')) {
          hasLoaderPath = true;
        }

        // Check for absolute paths to third-party libraries
        if (dylibPath.startsWith('/') &&
            !dylibPath.startsWith('/usr/lib/') &&
            !dylibPath.startsWith('/System/Library/') &&
            !dylibPath.startsWith('@loader_path/') &&
            !dylibPath.startsWith('@rpath/')) {

          const dylibName = path.basename(dylibPath);
          const localPath = path.join(path.dirname(binaryPath), dylibName);

          if (!exists(localPath) && !exists(dylibPath)) {
            errors.push(`Missing dependency: ${dylibName}`);
          }
        }
      }

      if (hasLoaderPath) {
        logger.log('Uses @loader_path', 'success');
      }

      if (errors.length === 0) {
        logger.log('All dependencies resolved', 'success');
      }
    }
  } catch (e) {
    warnings.push('otool not available');
  }

  return { errors, warnings };
}

/**
 * Windows verification
 */
function verifyWindows(binaryPath, binDir) {
  const errors = [];
  const warnings = [];

  // Check for Hamlib DLL
  const files = fs.readdirSync(binDir);
  const hamlibDll = files.find(f => /(lib)?hamlib(-\d+)?\.dll$/i.test(f));

  if (hamlibDll) {
    logger.log(`Found Hamlib DLL: ${hamlibDll}`, 'success');
  } else {
    errors.push('Hamlib DLL not found');
  }

  return { errors, warnings };
}

/**
 * Verify a single binary
 */
function verifyBinary(prebuild) {
  const platform = prebuild.platform.split('-')[0];

  let result;
  if (platform === 'linux') {
    result = verifyLinux(prebuild.path);
  } else if (platform === 'darwin') {
    result = verifyMac(prebuild.path);
  } else if (platform === 'win32') {
    result = verifyWindows(prebuild.path, prebuild.dir);
  } else {
    return { errors: [`Unsupported platform: ${platform}`], warnings: [] };
  }

  return result;
}

/**
 * Verify all platforms (CI mode)
 */
function verifyAll(prebuildsDir) {
  const expectedPlatforms = ['linux-x64', 'linux-arm64', 'darwin-arm64', 'win32-x64'];

  logger.step('Checking platform directories', 1, 2);

  const prebuilds = findPrebuilds(prebuildsDir);

  if (prebuilds.length === 0) {
    throw new Error('No prebuilt binaries found');
  }

  const foundPlatforms = new Set();
  let totalSize = 0;

  for (const prebuild of prebuilds) {
    const basePlatform = prebuild.platform.split('+')[0];
    foundPlatforms.add(basePlatform);

    const stats = fs.statSync(prebuild.path);
    totalSize += stats.size;

    logger.log(`${prebuild.platform}: ${path.basename(prebuild.path)} (${(stats.size / 1024).toFixed(1)} KB)`, 'success');

    // Check for bundled Hamlib
    const files = fs.readdirSync(prebuild.dir);
    const hasHamlib = files.some(f =>
      /^libhamlib\.(so(\..*)?|dylib)$/.test(f) ||
      /(lib)?hamlib(-\d+)?\.dll$/i.test(f)
    );

    if (hasHamlib) {
      logger.debug(`  Hamlib bundled`);
    }
  }

  const missing = expectedPlatforms.filter(p => !foundPlatforms.has(p));
  if (missing.length > 0) {
    throw new Error(`Missing platforms: ${missing.join(', ')}`);
  }

  logger.step('Verification summary', 2, 2);
  logger.log(`Platforms: ${foundPlatforms.size}/${expectedPlatforms.length}`, 'success');
  logger.log(`Total size: ${(totalSize / 1024 / 1024).toFixed(2)} MB`, 'success');

  // Check BUILD_INFO.txt
  const buildInfoPath = path.join(prebuildsDir, 'BUILD_INFO.txt');
  if (exists(buildInfoPath)) {
    logger.log('BUILD_INFO.txt present', 'success');
  }

  return true;
}

/**
 * Verify current platform
 */
function verifyCurrent(prebuildsDir) {
  const platformMap = {
    'win32': 'win32',
    'darwin': 'darwin',
    'linux': 'linux'
  };

  const platformFilter = platformMap[process.platform];
  if (!platformFilter) {
    throw new Error(`Unsupported platform: ${process.platform}`);
  }

  logger.step('Finding binaries', 1, 3);

  const prebuilds = findPrebuilds(prebuildsDir, platformFilter);

  if (prebuilds.length === 0) {
    throw new Error(`No prebuilt binaries found for ${process.platform}`);
  }

  logger.log(`Found ${prebuilds.length} binary(ies) for ${process.platform}`, 'info');

  logger.step('Verifying dependencies', 2, 3);

  let allPassed = true;
  const issues = [];

  for (const prebuild of prebuilds) {
    logger.log(`Checking ${prebuild.platform}/${prebuild.filename}`, 'info');

    const { errors, warnings } = verifyBinary(prebuild);

    if (errors.length > 0) {
      allPassed = false;
      errors.forEach(err => {
        logger.log(err, 'error');
        issues.push(err);
      });
    }

    if (warnings.length > 0) {
      warnings.forEach(warn => {
        logger.log(warn, 'warning');
      });
    }
  }

  logger.step('Complete', 3, 3);

  if (!allPassed) {
    throw new Error(`Verification failed: ${issues.length} issue(s) found`);
  }

  return true;
}

/**
 * Main function
 */
async function main() {
  const startTime = Date.now();
  const allMode = process.argv.includes('--all');

  logger.start();

  try {
    const prebuildsDir = path.join(__dirname, '..', 'prebuilds');

    if (!exists(prebuildsDir)) {
      throw new Error('prebuilds directory not found');
    }

    if (allMode) {
      await verifyAll(prebuildsDir);
    } else {
      await verifyCurrent(prebuildsDir);
    }

    const duration = (Date.now() - startTime) / 1000;
    logger.success(duration);

  } catch (error) {
    const solutions = [];

    if (error.message.includes('not found')) {
      solutions.push('Run prebuild first: npm run prebuild');
      solutions.push('Run bundling: npm run bundle');
    }

    if (error.message.includes('Missing dependencies')) {
      solutions.push('Re-run bundling: npm run bundle');
      solutions.push('Check Hamlib installation');
    }

    if (error.message.includes('Missing platforms')) {
      solutions.push('Build on missing platform');
      solutions.push('Check GitHub Actions logs');
    }

    logger.error('Verification failed', error.message, solutions);
    process.exit(1);
  }
}

if (require.main === module) {
  main().catch(err => {
    console.error('Unexpected error:', err);
    process.exit(1);
  });
}

module.exports = { verifyBinary, verifyAll, verifyCurrent };
