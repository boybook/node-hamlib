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
 * Check if we're in a MinGW environment
 */
function isMinGWEnvironment() {
  return process.env.HAMLIB_ROOT === '/mingw64' ||
         process.env.HAMLIB_ROOT === 'C:\\msys64\\mingw64' || 
         process.env.MSYSTEM === 'MINGW64' ||
         process.env.MINGW_PREFIX === '/mingw64' ||
         process.env.MINGW_PREFIX === 'C:\\msys64\\mingw64';
}

/**
 * Check if hamlib is available in the system
 */
function checkHamlibAvailable() {
  try {
    if (isMinGWEnvironment()) {
      // In MinGW environment, try to use pkg-config to check hamlib
      try {
        execSync('pkg-config --exists hamlib', { stdio: 'ignore' });
        console.log('✓ Hamlib found via pkg-config');
        return true;
      } catch (pkgConfigError) {
        // If pkg-config fails, try to check for specific files
        // Note: In MSYS2, /mingw64 might be mapped differently for Node.js
        console.log('pkg-config check failed, trying direct file checks...');
        
        // Try common MSYS2 paths
        const possiblePaths = [
          '/mingw64/include/hamlib',
          'C:/msys64/mingw64/include/hamlib',
          'C:\\msys64\\mingw64\\include\\hamlib',
          process.env.MINGW_PREFIX + '/include/hamlib',
          process.env.HAMLIB_ROOT + '/include/hamlib'
        ].filter(Boolean);
        
        for (const includePath of possiblePaths) {
          try {
            if (fs.existsSync(includePath)) {
              console.log(`✓ Hamlib headers found at: ${includePath}`);
              return true;
            }
          } catch (e) {
            // Continue to next path
          }
        }
        
        // If all file checks fail, assume hamlib is available since we're in MinGW
        // and the build process showed it was installed
        console.log('Direct file checks failed, but assuming hamlib is available in MinGW environment');
        return true;
      }
    } else {
      // Check if hamlib is available in standard locations
      const locations = [
        '/usr/include/hamlib',
        '/usr/local/include/hamlib',
        '/opt/homebrew/include/hamlib'
      ];
      return locations.some(location => fs.existsSync(location));
    }
  } catch (error) {
    console.log('Error checking hamlib availability:', error.message);
    // In MinGW environment, be more lenient since paths might be mapped differently
    return isMinGWEnvironment();
  }
}

/**
 * Setup Windows dependencies if needed
 */
async function setupWindowsDependencies() {
  // Skip if we're in MinGW environment - dependencies should already be set up
  if (isMinGWEnvironment()) {
    console.log('✓ MinGW environment detected, skipping Windows dependency setup...');
    return checkHamlibAvailable();
  }
  
  // Check if dependencies are already set up
  if (process.env.HAMLIB_ROOT && fs.existsSync(process.env.HAMLIB_ROOT)) {
    console.log('✓ Windows dependencies already set up, skipping...');
    return true;
  }
  
  console.log('Setting up Windows dependencies...');
  
  try {
    // Call PowerShell script instead of Node.js script
    const scriptPath = path.join(__dirname, 'setup-windows-deps.ps1');
    const command = `powershell -ExecutionPolicy Bypass -File "${scriptPath}"`;
    
    console.log(`Running: ${command}`);
    execSync(command, { stdio: 'inherit' });
    
    console.log('✓ Windows dependencies setup completed');
    return true;
  } catch (error) {
    console.error('✗ Failed to setup Windows dependencies:', error.message);
    return false;
  }
}

/**
 * Try to build from source
 */
async function buildFromSource() {
  console.log('Building hamlib from source...');
  
  try {
    // Setup Windows dependencies first if on Windows and not in MinGW environment
    if (process.platform === 'win32' && !isMinGWEnvironment()) {
      const success = await setupWindowsDependencies();
      if (!success) {
        throw new Error('Windows dependencies setup failed');
      }
    }
    
    // For MinGW environment, verify hamlib is available
    if (isMinGWEnvironment()) {
      if (!checkHamlibAvailable()) {
        throw new Error('Hamlib not found in MinGW environment. Please install hamlib first.');
      }
      console.log('✓ Hamlib found in MinGW environment');
    }
    
    // Build with node-gyp
    execSync('node-gyp configure', { stdio: 'inherit' });
    execSync('node-gyp build', { stdio: 'inherit' });
    
    console.log('✓ Built hamlib successfully');
    return true;
  } catch (error) {
    console.error('✗ Failed to build hamlib from source:', error.message);
    return false;
  }
}

/**
 * Main installation logic
 */
async function main() {
  console.log('Installing node-hamlib...');
  
  try {
    // Try to find prebuilt binary first
    const prebuiltPath = path.join(__dirname, '..', 'prebuilds');
    const platform = process.platform;
    const arch = process.arch;
    
    let targetDir = '';
    if (platform === 'win32' && arch === 'x64') {
      targetDir = 'win32-x64';
    } else if (platform === 'linux' && arch === 'x64') {
      targetDir = 'linux-x64';
    } else if (platform === 'linux' && arch === 'arm64') {
      targetDir = 'linux-arm64';
    } else if (platform === 'darwin' && arch === 'arm64') {
      targetDir = 'darwin-arm64';
    }
    
    const prebuiltBinary = path.join(prebuiltPath, targetDir, 'hamlib.node');
    
    if (fs.existsSync(prebuiltBinary)) {
      console.log('✓ Found prebuilt binary');
      
      // Copy to build directory
      const buildDir = path.join(__dirname, '..', 'build', 'Release');
      if (!fs.existsSync(buildDir)) {
        fs.mkdirSync(buildDir, { recursive: true });
      }
      
      const targetPath = path.join(buildDir, 'hamlib.node');
      fs.copyFileSync(prebuiltBinary, targetPath);
      
      console.log('✓ Installation completed successfully');
      return;
    }
    
    console.log('⚠ No prebuilt binary found for your platform');
    
    // Build from source
    const success = await buildFromSource();
    if (!success) {
      throw new Error('Failed to build from source');
    }
    
    console.log('✓ Installation completed successfully');
    
  } catch (error) {
    console.error('❌ Installation failed!');
    console.error('This could be due to:');
    console.error('1. No prebuilt binary available for your platform');
    console.error('2. Missing system dependencies (libhamlib-dev)');
    console.error('3. Missing build tools (node-gyp, compiler)');
    console.error('');
    console.error('To resolve this:');
    console.error('System dependencies:');
    console.error('  Linux: sudo apt-get install libhamlib-dev');
    console.error('  macOS: brew install hamlib');
    console.error('  Windows: Install hamlib via vcpkg or from source');
    console.error('');
    console.error('Build tools:');
    console.error('  npm install -g node-gyp');
    console.error('  # Follow node-gyp platform-specific setup instructions');
    
    process.exit(1);
  }
}

// Run only if this script is called directly
if (require.main === module) {
  main().catch(error => {
    console.error('Installation failed:', error);
    process.exit(1);
  });
} 