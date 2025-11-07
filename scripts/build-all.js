#!/usr/bin/env node

/**
 * Unified build script for node-hamlib
 * Handles complete build process: Hamlib setup → native compilation → bundling → verification
 *
 * Usage:
 *   npm run build:all                    - Full build for current platform
 *   npm run build:all -- --skip-hamlib   - Skip Hamlib build (use existing)
 *   npm run build:all -- --minimal       - Minimal Hamlib build
 *   npm run build:all -- --verify-only   - Only verify existing build
 *   npm run build:all -- --verbose       - Verbose output
 */

const fs = require('fs');
const path = require('path');
const { execSync, spawnSync } = require('child_process');
const { createLogger } = require('./logger');

const logger = createLogger('Building node-hamlib');

// Parse command line arguments
const args = {
  skipHamlib: process.argv.includes('--skip-hamlib'),
  minimal: process.argv.includes('--minimal'),
  verifyOnly: process.argv.includes('--verify-only'),
  verbose: process.argv.includes('--verbose'),
  skipVerify: process.argv.includes('--skip-verify')
};

// Set verbose flag
if (args.verbose) {
  process.env.VERBOSE = 'true';
}

function exists(p) {
  try {
    fs.accessSync(p);
    return true;
  } catch {
    return false;
  }
}

function exec(cmd, options = {}) {
  try {
    return execSync(cmd, {
      encoding: 'utf8',
      stdio: args.verbose ? 'inherit' : 'pipe',
      ...options
    });
  } catch (e) {
    throw new Error(`Command failed: ${cmd}\n${e.message}`);
  }
}

function checkCommand(cmd) {
  try {
    if (process.platform === 'win32') {
      execSync(`where ${cmd}`, { stdio: 'ignore' });
    } else {
      execSync(`which ${cmd}`, { stdio: 'ignore' });
    }
    return true;
  } catch {
    return false;
  }
}

/**
 * Step 1: Check build tools
 */
function checkBuildTools() {
  const requiredTools = {
    'node': 'Node.js',
    'npm': 'npm',
    'node-gyp': 'node-gyp (npm install -g node-gyp)'
  };

  // Platform-specific tools - use correct libtool command names
  let platformSpecific = [];
  if (process.platform === 'darwin') {
    // macOS uses glibtoolize
    platformSpecific = ['autoconf', 'automake', 'glibtoolize'];
  } else if (process.platform === 'linux') {
    // Linux uses libtoolize
    platformSpecific = ['autoconf', 'automake', 'libtoolize', 'make'];
  }

  const tools = { ...requiredTools };
  platformSpecific.forEach(tool => {
    tools[tool] = tool;
  });

  const missing = [];
  const found = [];

  for (const [cmd, name] of Object.entries(tools)) {
    if (checkCommand(cmd)) {
      found.push(name);
      logger.debug(`Found: ${name}`);
    } else {
      missing.push(name);
    }
  }

  if (found.length > 0) {
    logger.log(`Found ${found.length} required tool(s)`, 'success');
  }

  if (missing.length > 0) {
    const solutions = [];

    if (process.platform === 'darwin') {
      solutions.push('Install with Homebrew: brew install autoconf automake libtool');
    } else if (process.platform === 'linux') {
      solutions.push('Ubuntu/Debian: sudo apt install autoconf automake libtool build-essential');
      solutions.push('RHEL/CentOS: sudo yum install autoconf automake libtool gcc-c++');
    } else if (process.platform === 'win32') {
      solutions.push('Install Visual Studio Build Tools');
      solutions.push('Or install MinGW-w64');
    }

    throw new Error(`Missing required tools: ${missing.join(', ')}\n\nSolutions:\n${solutions.map(s => `  • ${s}`).join('\n')}`);
  }

  return true;
}

/**
 * Step 2: Build or download Hamlib
 */
async function setupHamlib() {
  if (args.skipHamlib) {
    logger.log('Skipping Hamlib setup (--skip-hamlib)', 'info');
    return;
  }

  // Check if Hamlib is already available
  const hamlibPrefix = process.env.HAMLIB_PREFIX;
  if (hamlibPrefix && exists(hamlibPrefix)) {
    logger.log(`Using existing Hamlib: ${hamlibPrefix}`, 'info');
    return;
  }

  if (process.platform === 'win32') {
    // Windows: Download precompiled Hamlib
    logger.startSpinner('Checking for Hamlib installation...');

    if (process.env.HAMLIB_ROOT && exists(process.env.HAMLIB_ROOT)) {
      logger.succeedSpinner('Using existing Hamlib installation');
      return;
    }

    logger.failSpinner('Hamlib not found');
    logger.warning('Windows users should set HAMLIB_ROOT or download Hamlib manually');
    logger.info('Download from: https://github.com/Hamlib/Hamlib/releases');
    throw new Error('HAMLIB_ROOT not set or Hamlib not found');
  } else {
    // Linux/macOS: Build from source
    logger.startSpinner('Building Hamlib from source...');

    try {
      const cwd = process.cwd();
      const hamlibPrefix = path.join(cwd, 'hamlib-build');

      const buildCmd = `node ${path.join(__dirname, 'build-hamlib.js')} --prefix=${hamlibPrefix}`;
      const buildArgs = args.minimal ? ' --minimal' : '';

      exec(buildCmd + buildArgs);

      logger.succeedSpinner('Hamlib built successfully');

      // Set environment variables for subsequent steps
      process.env.HAMLIB_PREFIX = hamlibPrefix;

      if (process.platform === 'linux') {
        const libPath = path.join(hamlibPrefix, 'lib');
        process.env.LD_LIBRARY_PATH = process.env.LD_LIBRARY_PATH
          ? `${libPath}:${process.env.LD_LIBRARY_PATH}`
          : libPath;
        process.env.PKG_CONFIG_PATH = process.env.PKG_CONFIG_PATH
          ? `${libPath}/pkgconfig:${process.env.PKG_CONFIG_PATH}`
          : `${libPath}/pkgconfig`;
      } else if (process.platform === 'darwin') {
        const libPath = path.join(hamlibPrefix, 'lib');
        process.env.DYLD_LIBRARY_PATH = process.env.DYLD_LIBRARY_PATH
          ? `${libPath}:${process.env.DYLD_LIBRARY_PATH}`
          : libPath;
        process.env.PKG_CONFIG_PATH = process.env.PKG_CONFIG_PATH
          ? `${libPath}/pkgconfig:${process.env.PKG_CONFIG_PATH}`
          : `${libPath}/pkgconfig`;
      }

      logger.debug(`Set HAMLIB_PREFIX=${hamlibPrefix}`);
    } catch (e) {
      logger.failSpinner('Hamlib build failed');
      throw e;
    }
  }
}

/**
 * Step 3: Compile native addon
 */
function compileNative() {
  logger.startSpinner('Running node-gyp configure...');

  try {
    exec('node-gyp configure');
    logger.succeedSpinner('Configuration complete');
  } catch (e) {
    logger.failSpinner('Configuration failed');
    throw e;
  }

  logger.startSpinner('Compiling native addon...');

  try {
    const startTime = Date.now();
    exec('node-gyp build');
    const duration = ((Date.now() - startTime) / 1000).toFixed(1);

    logger.succeedSpinner(`Compilation complete (${duration}s)`);

    // Check if binary was created
    const binaryPath = path.join(__dirname, '..', 'build', 'Release', 'hamlib.node');
    if (exists(binaryPath)) {
      const stats = fs.statSync(binaryPath);
      logger.log(`Binary created: ${(stats.size / 1024).toFixed(1)} KB`, 'success');
    }
  } catch (e) {
    logger.failSpinner('Compilation failed');
    throw e;
  }
}

/**
 * Step 4: Run prebuildify
 */
function runPrebuildify() {
  logger.startSpinner('Generating prebuilt binary...');

  try {
    if (process.platform === 'win32' && exists(path.join(__dirname, 'run-prebuildify.js'))) {
      // Windows: use special wrapper
      exec(`node ${path.join(__dirname, 'run-prebuildify.js')}`);
    } else {
      // Unix: direct prebuildify
      exec('npx prebuildify --napi --strip');
    }

    logger.succeedSpinner('Prebuilt binary generated');

    // List generated files
    const prebuildsDir = path.join(__dirname, '..', 'prebuilds');
    if (exists(prebuildsDir)) {
      const platforms = fs.readdirSync(prebuildsDir).filter(f => {
        const stat = fs.statSync(path.join(prebuildsDir, f));
        return stat.isDirectory();
      });

      if (platforms.length > 0) {
        logger.log(`Generated for: ${platforms.join(', ')}`, 'success');
      }
    }
  } catch (e) {
    logger.failSpinner('Prebuildify failed');
    throw e;
  }
}

/**
 * Step 5: Bundle dependencies
 */
async function bundleDependencies() {
  logger.startSpinner('Bundling dependencies...');

  try {
    const bundleScript = path.join(__dirname, 'bundle-deps.js');
    exec(`node ${bundleScript}`);

    logger.succeedSpinner('Dependencies bundled');
  } catch (e) {
    logger.failSpinner('Bundling failed');
    throw e;
  }
}

/**
 * Step 6: Verify build
 */
async function verifyBuild() {
  logger.startSpinner('Verifying build...');

  try {
    const verifyScript = path.join(__dirname, 'verify.js');
    exec(`node ${verifyScript}`);

    logger.succeedSpinner('Verification passed');
  } catch (e) {
    logger.failSpinner('Verification failed');
    logger.warning('Build may have issues, but continuing...');
    logger.debug(e.message);
  }
}

/**
 * Main build process
 */
async function main() {
  const startTime = Date.now();

  logger.start();

  try {
    if (args.verifyOnly) {
      // Verify-only mode
      logger.step('Verifying existing build', 1, 1);
      await verifyBuild();
    } else {
      // Full build process
      const totalSteps = args.skipVerify ? 5 : 6;
      let currentStep = 1;

      logger.step('Checking build tools', currentStep++, totalSteps);
      checkBuildTools();

      logger.step('Setting up Hamlib', currentStep++, totalSteps);
      await setupHamlib();

      logger.step('Compiling native addon', currentStep++, totalSteps);
      compileNative();

      logger.step('Generating prebuilt binary', currentStep++, totalSteps);
      runPrebuildify();

      logger.step('Bundling dependencies', currentStep++, totalSteps);
      await bundleDependencies();

      if (!args.skipVerify) {
        logger.step('Verifying build', currentStep++, totalSteps);
        await verifyBuild();
      }
    }

    const duration = (Date.now() - startTime) / 1000;
    logger.success(duration);

  } catch (error) {
    const solutions = [];

    // Provide context-specific solutions
    if (error.message.includes('node-gyp')) {
      solutions.push('Install node-gyp: npm install -g node-gyp');
      solutions.push('Install build tools for your platform');
    }

    if (error.message.includes('Hamlib')) {
      if (process.platform === 'win32') {
        solutions.push('Download Hamlib: https://github.com/Hamlib/Hamlib/releases');
        solutions.push('Set HAMLIB_ROOT environment variable');
      } else {
        solutions.push('Install build tools: autoconf, automake, libtool');
        solutions.push('Check network connection (git clone required)');
      }
    }

    if (error.message.includes('prebuildify')) {
      solutions.push('Install prebuildify: npm install prebuildify');
      solutions.push('Check that compilation succeeded');
    }

    if (error.message.includes('binding.gyp')) {
      solutions.push('Ensure Hamlib headers are installed');
      solutions.push('Set HAMLIB_PREFIX or HAMLIB_ROOT environment variable');
    }

    logger.error('Build failed', error.message, solutions);
    process.exit(1);
  }
}

if (require.main === module) {
  main().catch(err => {
    console.error('Unexpected error:', err);
    process.exit(1);
  });
}

module.exports = { main };
