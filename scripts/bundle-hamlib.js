#!/usr/bin/env node
/*
  Bundle hamlib runtime library into prebuilds/<platform>-<arch>/ alongside node.napi.node,
  and adjust binary references so the addon loads the bundled library at runtime.

  Supported: macOS (dylib via Homebrew), Linux (so via ldconfig/common paths).
  Windows: prints a note (not implemented yet).
*/

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

function log(msg) { console.log(msg); }
function warn(msg) { console.warn(msg); }
function exists(p) { try { fs.accessSync(p); return true; } catch { return false; } }

function getTargetDir() {
  const plat = process.platform; // 'darwin' | 'linux' | 'win32'
  const arch = process.arch; // 'arm64' | 'x64' | ...
  let platKey;
  if (plat === 'darwin') platKey = 'darwin';
  else if (plat === 'linux') platKey = 'linux';
  else if (plat === 'win32') platKey = 'win32';
  else throw new Error(`Unsupported platform: ${plat}`);

  const dir = path.join(__dirname, '..', 'prebuilds', `${platKey}-${arch}`);
  if (!exists(dir)) throw new Error(`Target prebuilds directory not found: ${dir}. Run: npx prebuildify --napi --strip`);
  const nodeBin = path.join(dir, 'node.napi.node');
  if (!exists(nodeBin)) throw new Error(`Binary not found: ${nodeBin}. Ensure prebuildify ran successfully.`);
  return { dir, nodeBin, plat: platKey };
}

/**
 * Get dependencies of a shared library using ldd
 * @param {string} libPath - Path to the .so file
 * @param {boolean} excludeSystemLibs - Whether to exclude system libraries
 * @returns {Array<{name: string, path: string}>} Array of dependency objects
 */
function getDependencies(libPath, excludeSystemLibs = true) {
  // System libraries that should NOT be bundled (always available on Linux)
  const systemLibs = [
    'linux-vdso.so',
    'libc.so',
    'libm.so',
    'libpthread.so',
    'libdl.so',
    'librt.so',
    'libgcc_s.so',
    'libstdc++.so',
    'ld-linux',
    '/lib64/ld-linux',
    '/lib/ld-linux'
  ];

  try {
    const out = execSync(`ldd ${JSON.stringify(libPath)}`, {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore']
    }).toString();

    const deps = [];
    for (const line of out.split('\n')) {
      // Parse lines like: "libusb-1.0.so.0 => /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 (0x00007f...)"
      const match = line.match(/^\s*(\S+)\s+=>\s+(\S+)/);
      if (!match) continue;

      const [, libName, resolvedPath] = match;

      // Skip "not found" entries
      if (resolvedPath === 'not' || !resolvedPath || resolvedPath.startsWith('(')) {
        continue;
      }

      // Skip system libraries if requested
      if (excludeSystemLibs && systemLibs.some(sys => libName.includes(sys))) {
        continue;
      }

      // Skip if path points to standard system directories (extra safety)
      if (excludeSystemLibs && (
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/libc') ||
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/libm') ||
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/libpthread') ||
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/libdl') ||
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/librt') ||
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/libgcc_s') ||
        resolvedPath.startsWith('/lib/x86_64-linux-gnu/libstdc++') ||
        resolvedPath.startsWith('/lib64/libc') ||
        resolvedPath.startsWith('/lib64/libm') ||
        resolvedPath.startsWith('/lib64/libpthread')
      )) {
        continue;
      }

      deps.push({ name: libName, path: resolvedPath });
    }

    return deps;
  } catch (e) {
    warn(`Failed to get dependencies for ${libPath}: ${e.message}`);
    return [];
  }
}

/**
 * Bundle libhamlib and all its transitive dependencies for Linux
 * @param {string} dir - Target prebuilds directory
 * @returns {boolean} Success status
 */
function bundleLinux(dir) {
  log('[Linux] Starting dependency bundling...');

  // Step 1: Find libhamlib.so
  let libPath = '';
  try {
    const out = execSync('ldconfig -p', { stdio: ['ignore', 'pipe', 'ignore'] }).toString();
    const line = out.split('\n').find(l => /libhamlib\.so(\.|$)/.test(l));
    if (line) libPath = line.trim().split(' => ').pop();
  } catch {}
  if (!libPath) {
    const candidates = [
      '/usr/lib64/libhamlib.so', '/usr/lib64/libhamlib.so.4', '/usr/lib64/libhamlib.so.5',
      '/usr/lib/libhamlib.so', '/usr/lib/libhamlib.so.4', '/usr/lib/libhamlib.so.5',
      '/usr/local/lib/libhamlib.so', '/usr/local/lib/libhamlib.so.4', '/usr/local/lib/libhamlib.so.5'
    ];
    libPath = candidates.find(exists) || '';
  }
  if (!libPath) {
    warn('[Linux] hamlib runtime (libhamlib.so*) not found. Skipping bundle.');
    return false;
  }

  // Step 2: Copy libhamlib.so
  const dest = path.join(dir, path.basename(libPath));
  fs.copyFileSync(libPath, dest);
  log(`[Linux] ✓ Bundled ${path.basename(libPath)} -> ${dest}`);

  // Step 3: Find and bundle all transitive dependencies
  log('[Linux] Analyzing transitive dependencies...');
  const deps = getDependencies(libPath, true);

  if (deps.length === 0) {
    log('[Linux] No additional dependencies found (or all are system libraries)');
  } else {
    log(`[Linux] Found ${deps.length} transitive dependency(ies) to bundle:`);

    for (const dep of deps) {
      log(`[Linux]   - ${dep.name} (from ${dep.path})`);
      const depDest = path.join(dir, dep.name);

      try {
        fs.copyFileSync(dep.path, depDest);
        log(`[Linux]     ✓ Bundled to ${depDest}`);
      } catch (e) {
        warn(`[Linux]     ✗ Failed to bundle ${dep.name}: ${e.message}`);
      }
    }
  }

  // Step 4: Fix RUNPATH for all bundled .so files using patchelf
  log('[Linux] Setting RUNPATH=$ORIGIN for all bundled libraries...');

  try {
    // Check if patchelf is available
    execSync('which patchelf', { stdio: 'ignore' });
  } catch (e) {
    warn('[Linux] patchelf not found, skipping RUNPATH fix');
    warn('[Linux] Install patchelf: sudo apt-get install patchelf');
    return true; // Continue anyway, fix-linux-rpath.js will handle it
  }

  const soFiles = fs.readdirSync(dir).filter(f =>
    f.endsWith('.so') || /\.so\.\d+$/.test(f)
  );

  for (const soFile of soFiles) {
    const soPath = path.join(dir, soFile);
    try {
      execSync(`patchelf --set-rpath '$ORIGIN' ${JSON.stringify(soPath)}`, {
        stdio: 'ignore'
      });
      log(`[Linux]   ✓ Set RUNPATH=$ORIGIN for ${soFile}`);
    } catch (e) {
      warn(`[Linux]   ✗ Failed to set RUNPATH for ${soFile}: ${e.message}`);
    }
  }

  log('[Linux] ✓ Bundling completed successfully');
  return true;
}

/**
 * Get dependencies of a dylib using otool -L
 * @param {string} dylibPath - Path to the .dylib file
 * @param {boolean} excludeSystemLibs - Whether to exclude system libraries
 * @returns {Array<{name: string, path: string}>} Array of dependency objects
 */
function getDependenciesMac(dylibPath, excludeSystemLibs = true) {
  // System libraries that should NOT be bundled (always available on macOS)
  const systemLibsPrefixes = [
    '/usr/lib/',                  // System libraries
    '/System/Library/',           // System frameworks
    '@rpath/',                    // Already using rpath (might be system)
    'libSystem',                  // System core library
    '/Library/Developer/CommandLineTools/',  // Developer tools
  ];

  try {
    const out = execSync(`otool -L ${JSON.stringify(dylibPath)}`, {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore']
    }).toString();

    const deps = [];
    const lines = out.split('\n');

    for (const line of lines) {
      // Parse lines like: "  /opt/homebrew/lib/libusb-1.0.0.dylib (compatibility version 3.0.0, current version 3.0.0)"
      // Skip the first line which is the library itself
      const match = line.match(/^\s+(\S+\.dylib)\s+\(/);
      if (!match) continue;

      const dylibFullPath = match[1];

      // Skip if it's the library itself
      if (dylibFullPath === dylibPath || path.basename(dylibFullPath) === path.basename(dylibPath)) {
        continue;
      }

      // Skip system libraries if requested
      if (excludeSystemLibs && systemLibsPrefixes.some(prefix => dylibFullPath.startsWith(prefix))) {
        continue;
      }

      // Skip @rpath, @executable_path, @loader_path references (we'll handle these separately)
      if (dylibFullPath.startsWith('@')) {
        continue;
      }

      const libName = path.basename(dylibFullPath);
      deps.push({ name: libName, path: dylibFullPath });
    }

    return deps;
  } catch (e) {
    warn(`[macOS] Failed to get dependencies for ${dylibPath}: ${e.message}`);
    return [];
  }
}

/**
 * Bundle libhamlib and all its transitive dependencies for macOS
 * @param {string} dir - Target prebuilds directory
 * @param {string} nodeBin - Path to node.napi.node binary
 * @returns {boolean} Success status
 */
function bundleMac(dir, nodeBin) {
  log('[macOS] Starting dependency bundling...');

  // Step 1: Locate libhamlib.dylib via Homebrew
  let prefix = '';
  try {
    prefix = execSync('brew --prefix hamlib', { stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim();
  } catch {}

  const searchDirs = [];
  if (prefix) searchDirs.push(path.join(prefix, 'lib'));
  searchDirs.push('/opt/homebrew/lib', '/usr/local/lib');

  let dylibs = [];
  for (const d of searchDirs) {
    try {
      const names = fs.readdirSync(d).filter(n => /^libhamlib.*\.dylib$/.test(n));
      for (const n of names) dylibs.push(path.join(d, n));
      if (dylibs.length) break;
    } catch {}
  }

  if (!dylibs.length) {
    warn('[macOS] hamlib runtime (libhamlib*.dylib) not found. Skipping bundle.');
    return false;
  }

  // Prefer versioned dylib first
  dylibs.sort((a, b) => b.length - a.length);
  const libPath = dylibs[0];

  // Step 2: Copy libhamlib.dylib
  const dest = path.join(dir, path.basename(libPath));
  fs.copyFileSync(libPath, dest);
  log(`[macOS] ✓ Bundled ${path.basename(libPath)} -> ${dest}`);

  // Step 3: Find and bundle all transitive dependencies
  log('[macOS] Analyzing transitive dependencies...');
  const deps = getDependenciesMac(libPath, true);

  if (deps.length === 0) {
    log('[macOS] No additional dependencies found (or all are system libraries)');
  } else {
    log(`[macOS] Found ${deps.length} transitive dependency(ies) to bundle:`);

    for (const dep of deps) {
      log(`[macOS]   - ${dep.name} (from ${dep.path})`);
      const depDest = path.join(dir, dep.name);

      try {
        if (exists(dep.path)) {
          fs.copyFileSync(dep.path, depDest);
          log(`[macOS]     ✓ Bundled to ${depDest}`);
        } else {
          warn(`[macOS]     ✗ Source not found: ${dep.path}`);
        }
      } catch (e) {
        warn(`[macOS]     ✗ Failed to bundle ${dep.name}: ${e.message}`);
      }
    }
  }

  // Step 4: Rewrite install_name for node.napi.node to use @loader_path
  log('[macOS] Rewriting install_name references to @loader_path...');

  let oldRef = '';
  try {
    const out = execSync(`otool -L ${JSON.stringify(nodeBin)}`, { stdio: ['ignore', 'pipe', 'ignore'] }).toString();
    const line = out.split('\n').find(l => /libhamlib.*\.dylib/.test(l));
    if (line) {
      const match = line.match(/^\s+(\S+\.dylib)/);
      if (match) oldRef = match[1];
    }
  } catch {}

  if (!oldRef) {
    warn('[macOS] No libhamlib reference found in addon binary; skip install_name_tool');
  } else {
    const libName = path.basename(oldRef);
    const localLib = path.join(dir, libName);

    if (!exists(localLib)) {
      warn(`[macOS] Expected bundled ${libName} not found at ${localLib}`);
    } else {
      try {
        execSync(`install_name_tool -change ${JSON.stringify(oldRef)} ${JSON.stringify('@loader_path/' + libName)} ${JSON.stringify(nodeBin)}`);
        log(`[macOS]   ✓ Rewrote ${libName} reference in node.napi.node`);
      } catch (e) {
        warn(`[macOS]   ✗ install_name_tool failed: ${e.message}`);
      }
    }
  }

  // Step 5: Fix install_name for all bundled dylibs to use @loader_path
  log('[macOS] Fixing install_name for bundled dylibs...');

  const dylibFiles = fs.readdirSync(dir).filter(f => f.endsWith('.dylib'));

  for (const dylibFile of dylibFiles) {
    const dylibPath = path.join(dir, dylibFile);

    try {
      // Get dependencies of this dylib
      const out = execSync(`otool -L ${JSON.stringify(dylibPath)}`, {
        encoding: 'utf8',
        stdio: ['ignore', 'pipe', 'ignore']
      }).toString();

      const lines = out.split('\n');
      for (const line of lines) {
        const match = line.match(/^\s+(\S+\.dylib)\s+\(/);
        if (!match) continue;

        const depPath = match[1];
        const depName = path.basename(depPath);

        // Skip if it's the library itself
        if (depName === dylibFile) continue;

        // Skip if it's already using @loader_path
        if (depPath.startsWith('@loader_path')) continue;

        // Check if this dependency was bundled
        if (exists(path.join(dir, depName))) {
          try {
            execSync(`install_name_tool -change ${JSON.stringify(depPath)} ${JSON.stringify('@loader_path/' + depName)} ${JSON.stringify(dylibPath)}`, {
              stdio: 'ignore'
            });
            log(`[macOS]   ✓ Fixed ${depName} reference in ${dylibFile}`);
          } catch (e) {
            warn(`[macOS]   ✗ Failed to fix ${depName} in ${dylibFile}: ${e.message}`);
          }
        }
      }
    } catch (e) {
      warn(`[macOS]   ✗ Failed to process ${dylibFile}: ${e.message}`);
    }
  }

  log('[macOS] ✓ Bundling completed successfully');
  return true;
}

/**
 * Get dependencies of a DLL using dumpbin (Windows)
 * Recursively analyzes DLL dependencies similar to ldd on Linux
 * @param {string} dllPath - Path to the DLL file
 * @param {Array<string>} searchPaths - Directories to search for dependencies
 * @param {boolean} excludeSystemDlls - Whether to exclude system DLLs
 * @param {Set<string>} visited - Set of already processed DLLs (for recursion control)
 * @returns {Array<{name: string, path: string}>} Array of dependency objects
 */
function getDependenciesWinRecursive(dllPath, searchPaths = [], excludeSystemDlls = true, visited = new Set()) {
  // Windows system DLLs that should NOT be bundled (always available on Windows)
  const systemDllPatterns = [
    /^kernel32\.dll$/i,
    /^user32\.dll$/i,
    /^advapi32\.dll$/i,
    /^ws2_32\.dll$/i,
    /^winmm\.dll$/i,
    /^msvcrt\.dll$/i,
    /^msvcp\d+\.dll$/i,    // MSVC C++ runtime
    /^vcruntime\d+\.dll$/i, // Visual C++ runtime
    /^ucrtbase\.dll$/i,     // Universal CRT
    /^api-ms-win-.*\.dll$/i, // API sets
    /^ole32\.dll$/i,
    /^oleaut32\.dll$/i,
    /^shell32\.dll$/i,
    /^gdi32\.dll$/i,
    /^ntdll\.dll$/i,
    /^setupapi\.dll$/i,
    /^cfgmgr32\.dll$/i,
    /^bcrypt\.dll$/i,
    /^sechost\.dll$/i,
    /^rpcrt4\.dll$/i,
  ];

  const dllName = path.basename(dllPath).toLowerCase();

  // Avoid infinite recursion
  if (visited.has(dllName)) {
    return [];
  }
  visited.add(dllName);

  // Check if this is a system DLL
  if (excludeSystemDlls && systemDllPatterns.some(pattern => pattern.test(dllName))) {
    return [];
  }

  const deps = [];

  try {
    // Try to use dumpbin first (most reliable)
    let dumpbinAvailable = false;
    try {
      execSync('where dumpbin', { stdio: 'ignore' });
      dumpbinAvailable = true;
    } catch (e) {
      // dumpbin not in PATH, try to find it via vswhere
      try {
        const vsWherePath = path.join(
          process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)',
          'Microsoft Visual Studio', 'Installer', 'vswhere.exe'
        );

        if (exists(vsWherePath)) {
          const vsPath = execSync(`"${vsWherePath}" -latest -property installationPath`, {
            encoding: 'utf8',
            stdio: ['ignore', 'pipe', 'ignore']
          }).toString().trim();

          if (vsPath) {
            const dumpbinPath = path.join(vsPath, 'VC', 'Tools', 'MSVC');
            if (exists(dumpbinPath)) {
              // Find the latest MSVC version
              const versions = fs.readdirSync(dumpbinPath);
              if (versions.length > 0) {
                versions.sort().reverse();
                const dumpbin = path.join(dumpbinPath, versions[0], 'bin', 'Hostx64', 'x64', 'dumpbin.exe');
                if (exists(dumpbin)) {
                  process.env.PATH = `${path.dirname(dumpbin)};${process.env.PATH}`;
                  dumpbinAvailable = true;
                }
              }
            }
          }
        }
      } catch (e2) {
        // vswhere not available or failed
      }
    }

    if (dumpbinAvailable) {
      // Use dumpbin to get actual dependencies
      const result = execSync(`dumpbin /dependents "${dllPath}"`, {
        encoding: 'utf8',
        stdio: ['ignore', 'pipe', 'ignore']
      }).toString();

      // Parse dumpbin output
      // Output format:
      //   Image has the following dependencies:
      //     libusb-1.0.dll
      //     KERNEL32.dll
      //     ...

      const lines = result.split('\n');
      let inDepsSection = false;

      for (const line of lines) {
        if (line.includes('has the following dependencies')) {
          inDepsSection = true;
          continue;
        }

        if (!inDepsSection) continue;

        // Stop at summary line
        if (line.includes('Summary') || line.trim() === '') {
          break;
        }

        // Extract DLL name
        const match = line.match(/^\s+(\S+\.dll)\s*$/i);
        if (!match) continue;

        const depName = match[1].toLowerCase();

        // Skip if already visited
        if (visited.has(depName)) {
          continue;
        }

        // Skip system DLLs
        if (excludeSystemDlls && systemDllPatterns.some(pattern => pattern.test(depName))) {
          continue;
        }

        // Try to find the DLL in search paths
        let depPath = '';
        for (const searchPath of searchPaths) {
          const candidate = path.join(searchPath, depName);
          if (exists(candidate)) {
            depPath = candidate;
            break;
          }
        }

        if (depPath) {
          deps.push({ name: depName, path: depPath });

          // Recursively get dependencies of this DLL
          const subDeps = getDependenciesWinRecursive(depPath, searchPaths, excludeSystemDlls, visited);
          deps.push(...subDeps);
        }
      }
    } else {
      // Fallback: directory scanning with pattern matching (old behavior)
      warn('[Windows] dumpbin not available, falling back to pattern matching');
      warn('[Windows] This may miss some dependencies. Install Visual Studio Build Tools for better results.');
    }
  } catch (e) {
    warn(`[Windows] Failed to analyze dependencies for ${dllName}: ${e.message}`);
  }

  return deps;
}

/**
 * Legacy fallback: Get dependencies by directory scanning (less reliable)
 * @param {string} binDir - Directory containing DLLs
 * @param {boolean} excludeSystemDlls - Whether to exclude system DLLs
 * @returns {Array<{name: string, path: string}>} Array of dependency objects
 */
function getDependenciesWinFallback(binDir, excludeSystemDlls = true) {
  // Windows system DLLs that should NOT be bundled
  const systemDllPatterns = [
    /^kernel32\.dll$/i,
    /^user32\.dll$/i,
    /^advapi32\.dll$/i,
    /^ws2_32\.dll$/i,
    /^winmm\.dll$/i,
    /^msvcrt\.dll$/i,
    /^msvcp\d+\.dll$/i,
    /^vcruntime\d+\.dll$/i,
    /^ucrtbase\.dll$/i,
    /^api-ms-win-.*\.dll$/i,
    /^ole32\.dll$/i,
    /^oleaut32\.dll$/i,
    /^shell32\.dll$/i,
    /^gdi32\.dll$/i,
    /^ntdll\.dll$/i,
  ];

  // Third-party DLL patterns we want to bundle
  const bundlePatterns = [
    /^libusb-1\.0.*\.dll$/i,
    /^libwinpthread-.*\.dll$/i,
    /^libgcc_s_.*\.dll$/i,
    /^libstdc\+\+-.*\.dll$/i,
    /^libiconv-.*\.dll$/i,
    /^libz.*\.dll$/i,
    /^zlib.*\.dll$/i,
    /^libindi.*\.dll$/i,
  ];

  try {
    if (!exists(binDir)) {
      return [];
    }

    const files = fs.readdirSync(binDir).filter(n => /\.dll$/i.test(n));
    const deps = [];

    for (const file of files) {
      // Skip if it's a system DLL
      if (excludeSystemDlls && systemDllPatterns.some(pattern => pattern.test(file))) {
        continue;
      }

      // Include if it matches bundle patterns
      if (bundlePatterns.some(pattern => pattern.test(file))) {
        const fullPath = path.join(binDir, file);
        if (exists(fullPath)) {
          deps.push({ name: file, path: fullPath });
        }
      }
    }

    return deps;
  } catch (e) {
    warn(`[Windows] Failed to scan dependencies in ${binDir}: ${e.message}`);
    return [];
  }
}

/**
 * Bundle libhamlib and all its dependencies for Windows
 * @param {string} dir - Target prebuilds directory
 * @returns {boolean} Success status
 */
function bundleWin(dir) {
  log('[Windows] Starting dependency bundling...');

  // Step 1: Locate hamlib DLL
  const candidates = [];
  const envRoot = process.env.HAMLIB_ROOT || '';
  const vcpkgRoot = process.env.VCPKG_ROOT || process.env.VCPKG_INSTALLATION_ROOT || '';

  // Preferred: HAMLIB_ROOT/bin
  if (envRoot) candidates.push(path.join(envRoot, 'bin'));
  // vcpkg typical: <vcpkg>/installed/x64-windows/bin
  if (vcpkgRoot) candidates.push(path.join(vcpkgRoot, 'installed', 'x64-windows', 'bin'));
  // Common fallbacks
  candidates.push(
    'C:/hamlib/bin',
    'C:/Program Files/Hamlib/bin',
    'C:/Program Files (x86)/Hamlib/bin'
  );

  function findDll(searchDir) {
    try {
      const files = fs.readdirSync(searchDir);
      // Match typical hamlib DLL names: hamlib-4.dll, libhamlib-4.dll, hamlib.dll
      const dll = files.find(n => /(lib)?hamlib(-\d+)?\.dll$/i.test(n));
      return dll ? path.join(searchDir, dll) : '';
    } catch {
      return '';
    }
  }

  let dllPath = '';
  let binDir = '';
  for (const d of candidates) {
    dllPath = findDll(d);
    if (dllPath) {
      binDir = d;
      break;
    }
  }

  if (!dllPath) {
    warn('[Windows] hamlib runtime DLL not found (set HAMLIB_ROOT or ensure vcpkg paths). Skipping bundle.');
    return false;
  }

  // Step 2: Copy hamlib DLL
  const dest = path.join(dir, path.basename(dllPath));
  fs.copyFileSync(dllPath, dest);
  log(`[Windows] ✓ Bundled ${path.basename(dllPath)} -> ${dest}`);

  // Step 3: Find and bundle all transitive dependencies using recursive analysis
  log('[Windows] Analyzing transitive dependencies recursively...');

  // Build search paths for dependencies
  const searchPaths = [binDir];

  // Add system paths
  if (process.env.PATH) {
    const systemPaths = process.env.PATH.split(';').filter(p => p.trim());
    searchPaths.push(...systemPaths);
  }

  // Try recursive dependency analysis with dumpbin
  const deps = getDependenciesWinRecursive(dllPath, searchPaths, true, new Set());

  if (deps.length === 0) {
    log('[Windows] No additional dependencies found (or all are system DLLs)');

    // If dumpbin failed, try fallback method
    log('[Windows] Trying fallback pattern-based scan...');
    const fallbackDeps = getDependenciesWinFallback(binDir, true);

    if (fallbackDeps.length > 0) {
      log(`[Windows] Found ${fallbackDeps.length} dependency(ies) via fallback method:`);

      for (const dep of fallbackDeps) {
        log(`[Windows]   - ${dep.name} (from ${dep.path})`);
        const depDest = path.join(dir, dep.name);

        try {
          fs.copyFileSync(dep.path, depDest);
          log(`[Windows]     ✓ Bundled to ${depDest}`);
        } catch (e) {
          warn(`[Windows]     ✗ Failed to bundle ${dep.name}: ${e.message}`);
        }
      }
    }
  } else {
    // Remove duplicates (deps may have duplicate entries from recursion)
    const uniqueDeps = [];
    const seen = new Set();

    for (const dep of deps) {
      const key = dep.name.toLowerCase();
      if (!seen.has(key)) {
        seen.add(key);
        uniqueDeps.push(dep);
      }
    }

    log(`[Windows] Found ${uniqueDeps.length} transitive dependency(ies) to bundle:`);

    for (const dep of uniqueDeps) {
      log(`[Windows]   - ${dep.name} (from ${dep.path})`);
      const depDest = path.join(dir, dep.name);

      try {
        // Skip if already bundled (e.g., the main hamlib DLL)
        if (exists(depDest) && path.basename(dllPath).toLowerCase() === dep.name.toLowerCase()) {
          log(`[Windows]     ⊙ Already bundled (main DLL)`);
          continue;
        }

        fs.copyFileSync(dep.path, depDest);
        log(`[Windows]     ✓ Bundled to ${depDest}`);
      } catch (e) {
        warn(`[Windows]     ✗ Failed to bundle ${dep.name}: ${e.message}`);
      }
    }
  }

  log('[Windows] ✓ Bundling completed successfully');
  return true;
}

function main() {
  const { dir, nodeBin, plat } = getTargetDir();
  log(`Target prebuilds dir: ${dir}`);
  let ok = false;
  if (plat === 'linux') ok = bundleLinux(dir);
  else if (plat === 'darwin') ok = bundleMac(dir, nodeBin);
  else if (plat === 'win32') {
    ok = bundleWin(dir);
  }
  if (!ok) {
    warn('Bundling completed with warnings.');
    process.exit(0);
  }
}

if (require.main === module) {
  try { main(); }
  catch (e) { console.error(e.message); process.exit(1); }
}
