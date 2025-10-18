#!/usr/bin/env node
/**
 * Simplified Windows dependency bundling using objdump
 *
 * This script uses objdump to enumerate DLL dependencies and copies them
 * to the prebuilds directory. It follows the principle from the documentation:
 * only bundle non-system DLLs, avoid complex filtering.
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

function log(msg) {
  console.log(`[bundle-windows] ${msg}`);
}

function error(msg) {
  console.error(`[bundle-windows] ❌ ${msg}`);
}

function success(msg) {
  console.log(`[bundle-windows] ✅ ${msg}`);
}

function exists(p) {
  try {
    fs.accessSync(p);
    return true;
  } catch {
    return false;
  }
}

/**
 * Get target directory based on architecture
 */
function getTargetDir() {
  const arch = process.arch; // 'x64' | 'arm64'
  const platform = 'win32';
  const dir = path.join(__dirname, '..', 'prebuilds', `${platform}-${arch}`);

  if (!exists(dir)) {
    error(`Target directory not found: ${dir}`);
    error('Run prebuildify first');
    process.exit(1);
  }

  const nodeBin = path.join(dir, 'node.napi.node');
  if (!exists(nodeBin)) {
    error(`Binary not found: ${nodeBin}`);
    error('Run prebuildify first');
    process.exit(1);
  }

  return { dir, nodeBin };
}

/**
 * Find hamlib DLL in standard locations
 */
function findHamlibDll() {
  const envRoot = process.env.HAMLIB_ROOT || '';
  const candidates = [];

  // Preferred: HAMLIB_ROOT/bin
  if (envRoot) {
    candidates.push(path.join(envRoot, 'bin'));
  }

  // Common fallbacks
  candidates.push(
    'C:/hamlib/bin',
    'C:/Program Files/Hamlib/bin',
    'C:/Program Files (x86)/Hamlib/bin'
  );

  for (const dir of candidates) {
    try {
      const files = fs.readdirSync(dir);
      // Match typical hamlib DLL names: hamlib-4.dll, libhamlib-4.dll, etc.
      const dll = files.find(n => /(lib)?hamlib(-\d+)?\.dll$/i.test(n));
      if (dll) {
        const dllPath = path.join(dir, dll);
        log(`Found hamlib DLL: ${dllPath}`);
        return { dllPath, binDir: dir };
      }
    } catch {
      // Directory doesn't exist, continue
    }
  }

  return null;
}

/**
 * Get DLL dependencies using objdump
 */
function getDependencies(dllPath) {
  try {
    // First try dumpbin (from Visual Studio)
    try {
      execSync('where dumpbin', { stdio: 'ignore' });
      const output = execSync(`dumpbin /dependents "${dllPath}"`, {
        encoding: 'utf8',
        stdio: ['ignore', 'pipe', 'ignore']
      }).toString();

      const deps = [];
      const lines = output.split('\n');
      let inDepsSection = false;

      for (const line of lines) {
        if (line.includes('has the following dependencies')) {
          inDepsSection = true;
          continue;
        }
        if (!inDepsSection) continue;
        if (line.includes('Summary')) break;

        const match = line.match(/^\s+(\S+\.dll)\s*$/i);
        if (match) {
          deps.push(match[1].toLowerCase());
        }
      }

      return deps;
    } catch {
      // dumpbin not available, try objdump
    }

    // Fallback to objdump (from MinGW)
    const output = execSync(`objdump -p "${dllPath}"`, {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore']
    }).toString();

    const deps = [];
    const lines = output.split('\n');

    for (const line of lines) {
      const match = line.match(/DLL Name:\s*(\S+\.dll)/i);
      if (match) {
        deps.push(match[1].toLowerCase());
      }
    }

    return deps;
  } catch (e) {
    error(`Failed to get dependencies: ${e.message}`);
    return [];
  }
}

/**
 * Check if a DLL is a Windows system DLL
 * Simplified approach: system DLLs are typically in C:\Windows
 */
function isSystemDll(dllName) {
  const systemDllPatterns = [
    /^kernel32\.dll$/i,
    /^kernelbase\.dll$/i,
    /^user32\.dll$/i,
    /^advapi32\.dll$/i,
    /^ws2_32\.dll$/i,
    /^msvcrt\.dll$/i,
    /^msvcp\d+.*\.dll$/i,
    /^vcruntime\d+\.dll$/i,
    /^ucrtbase\.dll$/i,
    /^api-ms-win-.*\.dll$/i,
    /^ntdll\.dll$/i,
    /^shell32\.dll$/i,
    /^ole32\.dll$/i,
    /^gdi32\.dll$/i,
  ];

  return systemDllPatterns.some(pattern => pattern.test(dllName));
}

/**
 * Main bundling function
 */
function main() {
  log('Starting Windows dependency bundling...');
  log('');

  const { dir, nodeBin } = getTargetDir();
  log(`Target directory: ${dir}`);

  // Find hamlib DLL
  const hamlibInfo = findHamlibDll();
  if (!hamlibInfo) {
    error('Hamlib DLL not found');
    error('Set HAMLIB_ROOT environment variable or install to standard location');
    process.exit(1);
  }

  const { dllPath, binDir } = hamlibInfo;

  // Copy hamlib DLL
  const hamlibDest = path.join(dir, path.basename(dllPath));
  fs.copyFileSync(dllPath, hamlibDest);
  success(`Copied ${path.basename(dllPath)} to ${dir}`);
  log('');

  // Get dependencies
  log('Analyzing dependencies with objdump/dumpbin...');
  const deps = getDependencies(dllPath);

  if (deps.length === 0) {
    log('No dependencies found');
  } else {
    log(`Found ${deps.length} dependencies`);
    log('');

    // Filter and copy dependencies
    const toCopy = [];
    const skipped = [];

    for (const dep of deps) {
      if (isSystemDll(dep)) {
        skipped.push(`${dep} (system DLL)`);
        continue;
      }

      // Try to find the DLL in binDir
      const depPath = path.join(binDir, dep);
      if (exists(depPath)) {
        toCopy.push({ name: dep, path: depPath });
      } else {
        skipped.push(`${dep} (not found in ${binDir})`);
      }
    }

    if (toCopy.length > 0) {
      log(`Copying ${toCopy.length} non-system DLL(s):`);
      for (const { name, path: srcPath } of toCopy) {
        const dest = path.join(dir, name);
        try {
          fs.copyFileSync(srcPath, dest);
          log(`  ✓ ${name}`);
        } catch (e) {
          error(`  ✗ Failed to copy ${name}: ${e.message}`);
        }
      }
    }

    if (skipped.length > 0) {
      log('');
      log(`Skipped ${skipped.length} DLL(s):`);
      skipped.slice(0, 5).forEach(s => log(`  - ${s}`));
      if (skipped.length > 5) {
        log(`  ... and ${skipped.length - 5} more`);
      }
    }
  }

  log('');
  success('Bundling completed successfully');
  log('');

  // Verification
  log('Verification: Listing bundled files');
  log('================================================');
  const files = fs.readdirSync(dir).filter(f => f.endsWith('.dll') || f.endsWith('.node'));
  files.forEach(f => {
    const stats = fs.statSync(path.join(dir, f));
    const sizeMB = (stats.size / 1024 / 1024).toFixed(2);
    log(`  ${f} (${sizeMB} MB)`);
  });
  log('');

  // Verify node.napi.node dependencies
  log('Verification: Checking node.napi.node dependencies');
  log('================================================');
  try {
    const nodeDeps = getDependencies(nodeBin);
    log('Dependencies of node.napi.node:');
    nodeDeps.forEach(dep => {
      const inDir = exists(path.join(dir, dep));
      const status = isSystemDll(dep) ? '(system)' : (inDir ? '✓' : '✗ MISSING');
      log(`  ${dep} ${status}`);
    });
  } catch (e) {
    error(`Failed to verify: ${e.message}`);
  }

  log('');
  log('================================================');
  success('All done!');
}

// Run main
if (require.main === module) {
  try {
    main();
  } catch (e) {
    error(`Unexpected error: ${e.message}`);
    console.error(e.stack);
    process.exit(1);
  }
}
