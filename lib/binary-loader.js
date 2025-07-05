const fs = require('fs');
const path = require('path');
const os = require('os');

/**
 * Get the platform-specific binary file name
 * @returns {string} Platform-specific binary file name
 */
function getPlatformBinaryPath() {
  const platform = os.platform();
  const arch = os.arch();
  
  let platformTarget;
  
  switch (platform) {
    case 'win32':
      platformTarget = `win32-${arch}`;
      break;
    case 'darwin':
      platformTarget = `darwin-${arch}`;
      break;
    case 'linux':
      platformTarget = `linux-${arch}`;
      break;
    default:
      throw new Error(`Unsupported platform: ${platform}-${arch}`);
  }
  
  return path.join(__dirname, '..', 'prebuilds', platformTarget, 'hamlib.node');
}

/**
 * Find and load the native hamlib module
 * @returns {Object} Native hamlib module
 */
function loadNativeModule() {
  const possiblePaths = [
    // First try prebuilt binary
    getPlatformBinaryPath(),
    // Fallback to locally compiled binary
    path.join(__dirname, '..', 'build', 'Release', 'hamlib.node'),
    path.join(__dirname, '..', 'build', 'Debug', 'hamlib.node'),
    // Try different possible locations
    path.join(__dirname, '..', 'hamlib.node'),
    path.join(process.cwd(), 'hamlib.node'),
    path.join(process.cwd(), 'build', 'Release', 'hamlib.node'),
    path.join(process.cwd(), 'build', 'Debug', 'hamlib.node')
  ];
  
  let lastError;
  
  for (const binaryPath of possiblePaths) {
    try {
      if (fs.existsSync(binaryPath)) {
        console.log(`Loading hamlib native module from: ${binaryPath}`);
        return require(binaryPath);
      }
    } catch (error) {
      lastError = error;
      console.warn(`Failed to load hamlib from ${binaryPath}: ${error.message}`);
    }
  }
  
  // If we get here, we couldn't find any working binary
  const errorMessage = [
    'Failed to load hamlib native module.',
    'Tried the following paths:',
    ...possiblePaths.map(p => `  - ${p}`),
    '',
    'This may be due to one of the following reasons:',
    '1. The prebuilt binary is not available for your platform',
    '2. The module needs to be compiled from source',
    '3. Missing system dependencies (libhamlib)',
    '',
    'To compile from source, run:',
    '  npm run build',
    '',
    'To install system dependencies:',
    '  Linux: sudo apt-get install libhamlib-dev',
    '  macOS: brew install hamlib',
    '  Windows: Install hamlib via vcpkg or from source'
  ].join('\n');
  
  const error = new Error(errorMessage);
  if (lastError) {
    error.originalError = lastError;
  }
  
  throw error;
}

module.exports = {
  loadNativeModule,
  getPlatformBinaryPath
}; 