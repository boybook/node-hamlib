const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const { getPlatformBinaryPath } = require('../lib/binary-loader');

/**
 * Check if a prebuilt binary exists for the current platform
 */
function checkPrebuiltBinary() {
  try {
    const binaryPath = getPlatformBinaryPath();
    return fs.existsSync(binaryPath);
  } catch (error) {
    return false;
  }
}

/**
 * Check if we can build from source
 */
function canBuildFromSource() {
  try {
    // Check if binding.gyp exists
    const bindingGypPath = path.join(__dirname, '..', 'binding.gyp');
    if (!fs.existsSync(bindingGypPath)) {
      return false;
    }

    // Check if source files exist
    const srcPath = path.join(__dirname, '..', 'src');
    if (!fs.existsSync(srcPath)) {
      return false;
    }

    // Check if node-gyp is available
    try {
      execSync('node-gyp --version', { stdio: 'ignore' });
      return true;
    } catch (error) {
      return false;
    }
  } catch (error) {
    return false;
  }
}

/**
 * Try to build from source
 */
function buildFromSource() {
  console.log('Building hamlib from source...');
  
  try {
    // Clean previous builds
    try {
      execSync('node-gyp clean', { stdio: 'inherit' });
    } catch (error) {
      // Ignore clean errors
    }

    // Configure and build
    execSync('node-gyp configure', { stdio: 'inherit' });
    execSync('node-gyp build', { stdio: 'inherit' });
    
    console.log('✓ Successfully built hamlib from source');
    return true;
  } catch (error) {
    console.error('✗ Failed to build hamlib from source:', error.message);
    return false;
  }
}

/**
 * Main installation logic
 */
function main() {
  console.log('Installing node-hamlib...');
  
  // Check if prebuilt binary exists
  if (checkPrebuiltBinary()) {
    console.log('✓ Found prebuilt binary for your platform');
    return;
  }

  console.log('⚠ No prebuilt binary found for your platform');
  
  // Try to build from source
  if (canBuildFromSource()) {
    if (buildFromSource()) {
      return;
    }
  } else {
    console.log('⚠ Cannot build from source (missing dependencies)');
  }

  // Installation failed
  console.error('');
  console.error('❌ Installation failed!');
  console.error('');
  console.error('This could be due to:');
  console.error('1. No prebuilt binary available for your platform');
  console.error('2. Missing system dependencies (libhamlib-dev)');
  console.error('3. Missing build tools (node-gyp, compiler)');
  console.error('');
  console.error('To resolve this:');
  console.error('');
  console.error('System dependencies:');
  console.error('  Linux: sudo apt-get install libhamlib-dev');
  console.error('  macOS: brew install hamlib');
  console.error('  Windows: Install hamlib via vcpkg or from source');
  console.error('');
  console.error('Build tools:');
  console.error('  npm install -g node-gyp');
  console.error('  # Follow node-gyp platform-specific setup instructions');
  console.error('');
  
  process.exit(1);
}

// Run only if this script is called directly
if (require.main === module) {
  main();
} 