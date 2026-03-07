#!/usr/bin/env node

/**
 * build-shim.js - Build the Hamlib shim layer
 *
 * Linux/macOS: Compiles as static library (.a) linked into the addon
 * Windows: Compiles as DLL (MinGW) for cross-compiler isolation
 *
 * Usage:
 *   node scripts/build-shim.js [--verbose]
 *
 * Environment variables:
 *   HAMLIB_ROOT    - Path to Hamlib installation (Windows)
 *   HAMLIB_PREFIX  - Path to Hamlib installation (Linux/macOS)
 *   MINGW_CC       - MinGW compiler path (Windows, defaults to gcc)
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');

const verbose = process.argv.includes('--verbose');
const projectRoot = path.join(__dirname, '..');
const shimDir = path.join(projectRoot, 'src', 'shim');
const buildDir = path.join(projectRoot, 'shim-build');
const isWindows = process.platform === 'win32';
const isMac = process.platform === 'darwin';

function log(msg) {
  console.log(`[build-shim] ${msg}`);
}

function exec(cmd, options = {}) {
  try {
    const result = execSync(cmd, {
      encoding: 'utf8',
      stdio: verbose ? 'inherit' : 'pipe',
      cwd: projectRoot,
      ...options
    });
    return result;
  } catch (err) {
    console.error(`Command failed: ${cmd}`);
    if (err.stderr) console.error(err.stderr);
    throw err;
  }
}

function ensureDir(dir) {
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }
}

/**
 * Detect MSYS2 MinGW64 prefix by checking common installation paths.
 * Returns the prefix path (e.g., 'C:/msys64/mingw64') or null.
 */
function findMsys2Prefix() {
  const candidates = [
    process.env.MINGW_PREFIX,  // Set inside MSYS2 MinGW shell
    'C:/msys64/mingw64',
    'D:/msys64/mingw64',
    'C:/msys64/ucrt64',
    'D:/msys64/ucrt64',
  ].filter(Boolean);

  for (const prefix of candidates) {
    const normalized = prefix.replace(/\\/g, '/');
    if (fs.existsSync(path.join(normalized, 'include', 'hamlib', 'rig.h'))) {
      return normalized;
    }
  }
  return null;
}

function findHamlibInclude() {
  if (isWindows) {
    const hamlibRoot = process.env.HAMLIB_ROOT;
    if (hamlibRoot && fs.existsSync(path.join(hamlibRoot, 'include'))) {
      return path.join(hamlibRoot, 'include');
    }
    // Try common paths
    const paths = ['C:/hamlib/include', 'C:/Program Files/Hamlib/include'];
    for (const p of paths) {
      if (fs.existsSync(p)) return p;
    }
    // Try MSYS2 MinGW as fallback
    const msys2 = findMsys2Prefix();
    if (msys2) return path.join(msys2, 'include');
    throw new Error('Cannot find Hamlib include directory. Set HAMLIB_ROOT or install via MSYS2 pacman.');
  }

  const prefix = process.env.HAMLIB_PREFIX;
  if (prefix && fs.existsSync(path.join(prefix, 'include'))) {
    return path.join(prefix, 'include');
  }

  // Check common paths
  const paths = isMac
    ? ['/opt/homebrew/include', '/opt/homebrew/opt/hamlib/include', '/usr/local/include']
    : ['/usr/local/include', '/usr/include'];

  for (const p of paths) {
    if (fs.existsSync(path.join(p, 'hamlib', 'rig.h'))) return p;
  }
  throw new Error('Cannot find Hamlib include directory. Set HAMLIB_PREFIX.');
}

function findHamlibLib() {
  if (isWindows) {
    const hamlibRoot = process.env.HAMLIB_ROOT;
    if (hamlibRoot) {
      // Check bin dir (where DLLs usually are on Windows)
      if (fs.existsSync(path.join(hamlibRoot, 'bin', 'libhamlib-4.dll'))) {
        return path.join(hamlibRoot, 'bin');
      }
      if (fs.existsSync(path.join(hamlibRoot, 'lib'))) {
        return path.join(hamlibRoot, 'lib');
      }
    }
    // Try MSYS2 MinGW as fallback
    const msys2 = findMsys2Prefix();
    if (msys2) return path.join(msys2, 'lib');
    throw new Error('Cannot find Hamlib library directory. Set HAMLIB_ROOT or install via MSYS2 pacman.');
  }

  const prefix = process.env.HAMLIB_PREFIX;
  if (prefix && fs.existsSync(path.join(prefix, 'lib'))) {
    return path.join(prefix, 'lib');
  }

  const paths = isMac
    ? ['/opt/homebrew/lib', '/opt/homebrew/opt/hamlib/lib', '/usr/local/lib']
    : ['/usr/local/lib', '/usr/lib', '/usr/lib/x86_64-linux-gnu'];

  for (const p of paths) {
    if (fs.existsSync(p)) return p;
  }
  throw new Error('Cannot find Hamlib library directory. Set HAMLIB_PREFIX.');
}

/**
 * Detect available Hamlib features by scanning rig.h for function declarations.
 * Returns an array of -DSHIM_HAS_* flags to pass to the compiler.
 */
function detectHamlibFeatures(includeDir) {
  const rigH = path.join(includeDir, 'hamlib', 'rig.h');
  if (!fs.existsSync(rigH)) {
    log('WARNING: rig.h not found, skipping feature detection');
    return [];
  }
  const content = fs.readFileSync(rigH, 'utf-8');
  const checks = [
    ['SHIM_HAS_GET_DEBUG',       'rig_get_debug'],
    ['SHIM_HAS_SPLIT_FREQ_MODE', 'rig_set_split_freq_mode'],
    ['SHIM_HAS_STOP_VOICE_MEM',  'rig_stop_voice_mem'],
    ['SHIM_HAS_LOCK_MODE',       'rig_set_lock_mode'],
    ['SHIM_HAS_CLOCK',           'rig_set_clock'],
    ['SHIM_HAS_VFO_INFO',        'rig_get_vfo_info'],
    ['SHIM_HAS_SEND_RAW',        'rig_send_raw'],
    ['SHIM_HAS_CONF2',           'rig_get_conf2'],
  ];
  const features = [];
  for (const [define, func] of checks) {
    if (content.includes(func)) {
      features.push(`-D${define}`);
    }
  }
  log(`Detected Hamlib features: ${features.length > 0 ? features.map(f => f.slice(2)).join(', ') : 'none'}`);
  return features;
}

function findMingwGcc() {
  // Try MINGW_CC env var first
  if (process.env.MINGW_CC) return process.env.MINGW_CC;

  // Try common MinGW gcc locations on Windows
  const candidates = [
    'gcc',  // In PATH (e.g., from MSYS2)
    'x86_64-w64-mingw32-gcc',
    'C:/msys64/mingw64/bin/gcc.exe',
    'C:/mingw64/bin/gcc.exe',
  ];

  for (const gcc of candidates) {
    try {
      execSync(`${gcc} --version`, { stdio: 'pipe', encoding: 'utf8' });
      return gcc;
    } catch { /* continue */ }
  }
  throw new Error('MinGW gcc not found. Install MSYS2/mingw-w64 or set MINGW_CC.');
}

function buildWindows() {
  log('Building shim DLL for Windows (MinGW)...');

  const gcc = findMingwGcc();
  log(`Using MinGW compiler: ${gcc}`);

  const includeDir = findHamlibInclude();
  const libDir = findHamlibLib();
  log(`Hamlib include: ${includeDir}`);
  log(`Hamlib lib: ${libDir}`);

  ensureDir(buildDir);

  const shimSrc = path.join(shimDir, 'hamlib_shim.c').replace(/\\/g, '/');
  const shimDll = path.join(buildDir, 'hamlib_shim.dll').replace(/\\/g, '/');
  const shimDef = path.join(buildDir, 'hamlib_shim.def').replace(/\\/g, '/');

  // Detect available Hamlib features
  const featureFlags = detectHamlibFeatures(includeDir);

  // Determine link strategy: DLL (hamlib-w64 package) vs static (MSYS2 pacman)
  const libDirFwd = libDir.replace(/\\/g, '/');
  const hamlibDll = path.join(libDir, 'libhamlib-4.dll').replace(/\\/g, '/');
  const hamlibStaticLib = path.join(libDir, 'libhamlib.a').replace(/\\/g, '/');
  // Also check bin/ sibling (hamlib-w64 puts DLLs in bin/)
  const hamlibDllInBin = path.join(libDir, '..', 'bin', 'libhamlib-4.dll').replace(/\\/g, '/');

  let linkArgs;
  if (fs.existsSync(hamlibDll)) {
    // DLL in lib dir
    linkArgs = [`-L${libDirFwd}`, '-lhamlib-4'];
    log('Linking against libhamlib-4.dll (from lib/)');
  } else if (fs.existsSync(hamlibDllInBin)) {
    // hamlib-w64 package: DLL in bin/, import lib in lib/
    const binDir = path.join(libDir, '..', 'bin').replace(/\\/g, '/');
    linkArgs = [`-L${binDir}`, `-L${libDirFwd}`, '-lhamlib-4'];
    log('Linking against libhamlib-4.dll (from bin/)');
  } else if (fs.existsSync(hamlibStaticLib)) {
    // MSYS2 pacman: static .a, need system libs
    linkArgs = [hamlibStaticLib, '-lws2_32', '-lwinmm', '-liphlpapi'];
    log('Linking statically against libhamlib.a (MSYS2)');
  } else {
    throw new Error(
      `Cannot find Hamlib library in ${libDir}. ` +
      'Expected libhamlib-4.dll or libhamlib.a.'
    );
  }

  // Compile shim DLL with MinGW
  const gccCmd = [
    gcc,
    '-shared', '-O2',
    '-DHAMLIB_SHIM_BUILD',
    ...featureFlags,
    `-I${includeDir.replace(/\\/g, '/')}`,
    '-o', shimDll,
    shimSrc,
    ...linkArgs,
    `-Wl,--output-def,${shimDef}`
  ].join(' ');

  log(`Compiling: ${gccCmd}`);
  exec(gccCmd);

  if (!fs.existsSync(shimDll.replace(/\//g, '\\'))) {
    throw new Error('Failed to create hamlib_shim.dll');
  }
  log(`Created: ${shimDll}`);

  // Generate MSVC import library from the DLL
  generateMsvcImportLib(shimDll, shimDef);

  // Copy shim DLL to build/Release dir if it exists (for local dev)
  const releaseDir = path.join(projectRoot, 'build', 'Release');
  if (fs.existsSync(releaseDir)) {
    fs.copyFileSync(
      shimDll.replace(/\//g, '\\'),
      path.join(releaseDir, 'hamlib_shim.dll')
    );
    log('Copied hamlib_shim.dll to build/Release/');
  }
}

function findMsvcLibExe() {
  // Search for lib.exe in Visual Studio installations
  const vsRoots = [
    'C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools',
    'C:/Program Files/Microsoft Visual Studio/2022/Community',
    'C:/Program Files/Microsoft Visual Studio/2022/Professional',
    'C:/Program Files/Microsoft Visual Studio/2022/Enterprise',
    'C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools',
    'C:/Program Files (x86)/Microsoft Visual Studio/2019/Community',
  ];

  for (const vsRoot of vsRoots) {
    const toolsDir = path.join(vsRoot, 'VC', 'Tools', 'MSVC');
    if (!fs.existsSync(toolsDir)) continue;
    const versions = fs.readdirSync(toolsDir).sort().reverse();
    for (const ver of versions) {
      const libExe = path.join(toolsDir, ver, 'bin', 'Hostx64', 'x64', 'lib.exe');
      if (fs.existsSync(libExe)) return libExe;
    }
  }
  return null;
}

function generateMsvcImportLib(dllPath, defPath) {
  log('Generating MSVC import library...');

  const libPath = path.join(buildDir, 'hamlib_shim.lib').replace(/\\/g, '/');

  // Try using lib.exe (from MSVC) - search in VS installation
  const libExe = findMsvcLibExe();
  if (libExe) {
    try {
      const libCmd = `"${libExe}" /def:${defPath} /out:${libPath} /machine:x64`;
      exec(libCmd);
      log(`Created MSVC import lib: ${libPath}`);
      return;
    } catch (e) {
      log(`lib.exe failed: ${e.message}, trying dlltool...`);
    }
  } else {
    // Try lib.exe from PATH (e.g., in VS Developer Command Prompt)
    try {
      const libCmd = `lib /def:${defPath} /out:${libPath} /machine:x64`;
      exec(libCmd);
      log(`Created MSVC import lib: ${libPath}`);
      return;
    } catch {
      log('lib.exe not found in PATH, trying dlltool...');
    }
  }

  // Fallback: use dlltool (from MinGW)
  try {
    const dlltoolCmd = `dlltool -d ${defPath} -l ${libPath} -D hamlib_shim.dll`;
    exec(dlltoolCmd);
    log(`Created import lib via dlltool: ${libPath}`);
  } catch {
    log('WARNING: Could not generate import library. Linking may fail.');
  }
}

function buildUnix() {
  const platform = isMac ? 'macOS' : 'Linux';
  log(`Building shim static library for ${platform}...`);

  const cc = process.env.CC || 'gcc';
  const includeDir = findHamlibInclude();
  const libDir = findHamlibLib();
  log(`Hamlib include: ${includeDir}`);
  log(`Hamlib lib: ${libDir}`);

  ensureDir(buildDir);

  // Detect available Hamlib features
  const featureFlags = detectHamlibFeatures(includeDir);

  const shimSrc = path.join(shimDir, 'hamlib_shim.c');
  const shimObj = path.join(buildDir, 'hamlib_shim.o');
  const shimLib = path.join(buildDir, 'libhamlib_shim.a');

  // Compile object file
  const ccCmd = [
    cc,
    '-c', '-fPIC', '-O2',
    '-DHAMLIB_SHIM_BUILD',
    ...featureFlags,
    `-I${includeDir}`,
    '-o', shimObj,
    shimSrc
  ].join(' ');

  log(`Compiling: ${ccCmd}`);
  exec(ccCmd);

  // Create static library
  const arCmd = `ar rcs ${shimLib} ${shimObj}`;
  log(`Archiving: ${arCmd}`);
  exec(arCmd);

  if (!fs.existsSync(shimLib)) {
    throw new Error('Failed to create libhamlib_shim.a');
  }
  log(`Created: ${shimLib}`);
}

// Main
try {
  log(`Platform: ${process.platform}, Arch: ${process.arch}`);

  if (isWindows) {
    buildWindows();
  } else {
    buildUnix();
  }

  log('Shim build completed successfully.');
} catch (err) {
  console.error(`[build-shim] ERROR: ${err.message}`);
  process.exit(1);
}
