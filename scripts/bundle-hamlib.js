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

function bundleLinux(dir) {
  // Try ldconfig first
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
    warn('hamlib runtime (libhamlib.so*) not found. Skipping bundle.');
    return false;
  }
  const dest = path.join(dir, path.basename(libPath));
  fs.copyFileSync(libPath, dest);
  log(`Bundled ${path.basename(libPath)} -> ${dest}`);
  return true;
}

function bundleMac(dir, nodeBin) {
  // Locate dylib via Homebrew
  let prefix = '';
  try { prefix = execSync('brew --prefix hamlib', { stdio: ['ignore', 'pipe', 'ignore'] }).toString().trim(); } catch {}
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
    warn('hamlib runtime (libhamlib*.dylib) not found. Skipping bundle.');
    return false;
  }
  // Prefer versioned dylib first
  dylibs.sort((a, b) => b.length - a.length);
  let bundled = false;
  for (const p of dylibs) {
    const dest = path.join(dir, path.basename(p));
    try { fs.copyFileSync(p, dest); bundled = true; log(`Bundled ${path.basename(p)} -> ${dest}`); } catch {}
  }
  if (!bundled) return false;

  // Rewrite install_name to @loader_path/<libname>
  let oldRef = '';
  try {
    const out = execSync(`otool -L ${JSON.stringify(nodeBin)}`, { stdio: ['ignore', 'pipe', 'ignore'] }).toString();
    const line = out.split('\n').find(l => /libhamlib.*\.dylib/.test(l));
    if (line) oldRef = line.trim().split(' ')[0];
  } catch {}
  if (!oldRef) {
    warn('No libhamlib reference found in addon binary; skip install_name_tool');
    return true;
  }
  const libName = path.basename(oldRef);
  const localLib = path.join(dir, libName);
  if (!exists(localLib)) {
    warn(`Expected bundled ${libName} not found at ${localLib}`);
    return true;
  }
  try {
    execSync(`install_name_tool -change ${JSON.stringify(oldRef)} ${JSON.stringify('@loader_path/' + libName)} ${JSON.stringify(nodeBin)}`);
    const verify = execSync(`otool -L ${JSON.stringify(nodeBin)}`, { stdio: ['ignore', 'pipe', 'ignore'] }).toString();
    log('Rewrote hamlib reference to @loader_path. Current deps:\n' + verify);
  } catch (e) {
    warn('install_name_tool failed: ' + e.message);
  }
  return true;
}

function main() {
  const { dir, nodeBin, plat } = getTargetDir();
  log(`Target prebuilds dir: ${dir}`);
  let ok = false;
  if (plat === 'linux') ok = bundleLinux(dir);
  else if (plat === 'darwin') ok = bundleMac(dir, nodeBin);
  else if (plat === 'win32') {
    warn('Windows bundling not implemented. Place hamlib-*.dll next to node.napi.node.');
    ok = true;
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

