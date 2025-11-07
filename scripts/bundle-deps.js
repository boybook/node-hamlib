#!/usr/bin/env node

/**
 * Unified cross-platform dependency bundling
 * Bundles non-system dependencies for Windows, macOS, and Linux
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const { createLogger } = require('./logger');

const logger = createLogger('Bundling Dependencies');

function exists(p) {
  try {
    fs.accessSync(p);
    return true;
  } catch {
    return false;
  }
}

function getTargetDir(platform = process.platform, arch = process.arch) {
  const platformMap = {
    'win32': 'win32',
    'darwin': 'darwin',
    'linux': 'linux'
  };

  const mappedPlatform = platformMap[platform] || platform;
  const dir = path.join(__dirname, '..', 'prebuilds', `${mappedPlatform}-${arch}`);
  const nodeBin = path.join(dir, 'node.napi.node');

  if (!exists(dir)) {
    throw new Error(`Target directory not found: ${dir}`);
  }

  if (!exists(nodeBin)) {
    throw new Error(`Binary not found: ${nodeBin}`);
  }

  return { dir, nodeBin };
}

/**
 * Windows bundling
 */
class WindowsBundler {
  constructor(targetDir, nodeBin) {
    this.targetDir = targetDir;
    this.nodeBin = nodeBin;
  }

  findHamlibDll() {
    const envRoot = process.env.HAMLIB_ROOT || '';
    const candidates = [];

    if (envRoot) {
      candidates.push(path.join(envRoot, 'bin'));
    }

    candidates.push(
      'C:/hamlib/bin',
      'C:/Program Files/Hamlib/bin',
      'C:/Program Files (x86)/Hamlib/bin'
    );

    for (const dir of candidates) {
      try {
        const files = fs.readdirSync(dir);
        const dll = files.find(n => /(lib)?hamlib(-\d+)?\.dll$/i.test(n));
        if (dll) {
          const dllPath = path.join(dir, dll);
          return { dllPath, binDir: dir };
        }
      } catch {
        // Directory doesn't exist
      }
    }

    return null;
  }

  getDependencies(dllPath) {
    try {
      // Try dumpbin first (MSVC)
      try {
        execSync('where dumpbin', { stdio: 'ignore' });
        const output = execSync(`dumpbin /dependents "${dllPath}"`, {
          encoding: 'utf8',
          stdio: ['ignore', 'pipe', 'ignore']
        });

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
        // dumpbin not available
      }

      // Fallback to objdump (MinGW)
      const output = execSync(`objdump -p "${dllPath}"`, {
        encoding: 'utf8',
        stdio: ['ignore', 'pipe', 'ignore']
      });

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
      logger.debug(`Failed to get dependencies: ${e.message}`);
      return [];
    }
  }

  isSystemDll(dllName) {
    const systemPatterns = [
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

    return systemPatterns.some(pattern => pattern.test(dllName));
  }

  async bundle() {
    // Find hamlib DLL
    const hamlibInfo = this.findHamlibDll();
    if (!hamlibInfo) {
      throw new Error('Hamlib DLL not found in standard locations');
    }

    const { dllPath, binDir } = hamlibInfo;
    logger.log(`Found Hamlib DLL: ${path.basename(dllPath)}`, 'success');

    // Copy hamlib DLL
    const hamlibDest = path.join(this.targetDir, path.basename(dllPath));
    fs.copyFileSync(dllPath, hamlibDest);
    logger.log(`Copied ${path.basename(dllPath)}`, 'success');

    // Get and copy dependencies
    const deps = this.getDependencies(dllPath);
    if (deps.length === 0) {
      logger.log('No additional dependencies found', 'info');
      return;
    }

    const toCopy = [];
    for (const dep of deps) {
      if (this.isSystemDll(dep)) continue;

      const depPath = path.join(binDir, dep);
      if (exists(depPath)) {
        toCopy.push({ name: dep, path: depPath });
      }
    }

    if (toCopy.length > 0) {
      for (const { name, path: srcPath } of toCopy) {
        const dest = path.join(this.targetDir, name);
        try {
          fs.copyFileSync(srcPath, dest);
          logger.log(`Copied ${name}`, 'success');
        } catch (e) {
          logger.log(`Failed to copy ${name}: ${e.message}`, 'error');
        }
      }
    }

    // Verify
    this.verify();
  }

  verify() {
    const nodeDeps = this.getDependencies(this.nodeBin);
    let missing = 0;

    for (const dep of nodeDeps) {
      if (this.isSystemDll(dep)) continue;

      const inDir = exists(path.join(this.targetDir, dep));
      if (!inDir) {
        logger.log(`Missing dependency: ${dep}`, 'warning');
        missing++;
      }
    }

    if (missing === 0) {
      logger.log('All dependencies resolved', 'success');
    } else {
      logger.log(`${missing} dependencies missing`, 'warning');
    }
  }
}

/**
 * macOS bundling
 */
class MacOSBundler {
  constructor(targetDir, nodeBin) {
    this.targetDir = targetDir;
    this.nodeBin = nodeBin;
  }

  checkDylibbundler() {
    try {
      execSync('which dylibbundler', { stdio: 'ignore' });
      return true;
    } catch {
      return false;
    }
  }

  getSearchPaths() {
    const arch = process.arch;
    const homebrewPrefix = arch === 'arm64' ? '/opt/homebrew' : '/usr/local';

    const paths = [];

    if (process.env.HAMLIB_PREFIX) {
      paths.push(path.join(process.env.HAMLIB_PREFIX, 'lib'));
    }

    paths.push(
      path.join(homebrewPrefix, 'opt/hamlib/lib'),
      path.join(homebrewPrefix, 'opt/fftw/lib'),
      path.join(homebrewPrefix, 'opt/gcc/lib/gcc/current'),
      path.join(homebrewPrefix, 'opt/gcc@14/lib/gcc/14'),
      path.join(homebrewPrefix, 'opt/gcc@13/lib/gcc/13'),
      path.join(homebrewPrefix, 'lib')
    );

    return paths.filter(p => exists(p));
  }

  async bundle() {
    if (!this.checkDylibbundler()) {
      throw new Error('dylibbundler not found');
    }

    const searchPaths = this.getSearchPaths();
    logger.debug(`Search paths: ${searchPaths.length} directories`);

    // Build dylibbundler command
    let cmd = `dylibbundler -x "${this.nodeBin}" -d "${this.targetDir}" -p "@loader_path/"`;

    for (const searchPath of searchPaths) {
      cmd += ` -s "${searchPath}"`;
    }

    cmd += ' -b';  // Overwrite existing files

    logger.startSpinner('Running dylibbundler...');

    try {
      execSync(cmd, {
        stdio: logger.verbose ? 'inherit' : 'pipe',
        encoding: 'utf8'
      });
      logger.succeedSpinner('dylibbundler completed');
    } catch (e) {
      logger.failSpinner('dylibbundler failed');
      throw e;
    }

    // Verify
    this.verify();
  }

  verify() {
    try {
      const output = execSync(`otool -L "${this.nodeBin}"`, { encoding: 'utf8' });
      const lines = output.split('\n');
      let hasLoaderPath = false;

      for (const line of lines) {
        if (line.includes('@loader_path')) {
          hasLoaderPath = true;
        }
        if (line.includes('libhamlib')) {
          logger.log(`Hamlib dependency: ${line.trim()}`, 'success');
        }
      }

      if (hasLoaderPath) {
        logger.log('Dependencies use @loader_path (correct)', 'success');
      }
    } catch (e) {
      logger.log(`Verification failed: ${e.message}`, 'warning');
    }
  }
}

/**
 * Linux bundling
 */
class LinuxBundler {
  constructor(targetDir, nodeBin) {
    this.targetDir = targetDir;
    this.nodeBin = nodeBin;
  }

  checkTools() {
    try {
      execSync('which patchelf', { stdio: 'ignore' });
      return true;
    } catch {
      return false;
    }
  }

  findHamlibSo() {
    // Check HAMLIB_PREFIX
    if (process.env.HAMLIB_PREFIX) {
      const paths = [
        path.join(process.env.HAMLIB_PREFIX, 'lib/libhamlib.so.4'),
        path.join(process.env.HAMLIB_PREFIX, 'lib/libhamlib.so')
      ];
      for (const p of paths) {
        if (exists(p)) return p;
      }
    }

    // Try ldconfig
    try {
      const output = execSync("ldconfig -p | grep 'libhamlib\\.so' | head -1 | awk '{print $NF}'", {
        encoding: 'utf8',
        shell: '/bin/bash'
      }).trim();
      if (output && exists(output)) return output;
    } catch {
      // ldconfig failed
    }

    // Common paths
    const commonPaths = [
      '/usr/lib64/libhamlib.so.4',
      '/usr/lib/libhamlib.so.4',
      '/usr/local/lib/libhamlib.so.4',
      '/usr/lib/x86_64-linux-gnu/libhamlib.so.4',
      '/usr/lib/aarch64-linux-gnu/libhamlib.so.4'
    ];

    for (const p of commonPaths) {
      if (exists(p)) return p;
    }

    return null;
  }

  isSystemLib(libname) {
    const systemPatterns = [
      'linux-vdso',
      'ld-linux',
      'libc.so',
      'libm.so',
      'libpthread.so',
      'libdl.so',
      'librt.so',
      'libgcc_s.so',
      'libstdc++.so'
    ];

    return systemPatterns.some(pattern => libname.includes(pattern));
  }

  async bundle() {
    if (!this.checkTools()) {
      throw new Error('patchelf not found');
    }

    // Find libhamlib.so
    const hamlibLib = this.findHamlibSo();
    if (!hamlibLib) {
      throw new Error('libhamlib.so not found');
    }

    logger.log(`Found libhamlib: ${path.basename(hamlibLib)}`, 'success');

    // Copy libhamlib.so
    const hamlibDest = path.join(this.targetDir, path.basename(hamlibLib));
    fs.copyFileSync(hamlibLib, hamlibDest);
    logger.log(`Copied ${path.basename(hamlibLib)}`, 'success');

    // Get dependencies
    try {
      const output = execSync(`ldd "${hamlibLib}"`, { encoding: 'utf8' });
      const lines = output.split('\n');

      for (const line of lines) {
        const match = line.match(/^\s*(\S+)\s+=>\s+(\S+)/);
        if (!match) continue;

        const [, libname, libpath] = match;

        if (this.isSystemLib(libname)) continue;
        if (libpath === 'not' || !exists(libpath)) continue;

        try {
          const dest = path.join(this.targetDir, path.basename(libpath));
          fs.copyFileSync(libpath, dest);
          logger.log(`Copied ${path.basename(libpath)}`, 'success');
        } catch (e) {
          logger.log(`Failed to copy ${libname}: ${e.message}`, 'warning');
        }
      }
    } catch (e) {
      logger.log('No additional dependencies found', 'info');
    }

    // Set RUNPATH
    this.setRunpath();

    // Verify
    this.verify();
  }

  setRunpath() {
    const files = fs.readdirSync(this.targetDir).filter(f =>
      f.endsWith('.node') || f.endsWith('.so') || f.match(/\.so\.\d+$/)
    );

    for (const file of files) {
      const filePath = path.join(this.targetDir, file);
      try {
        execSync(`patchelf --set-rpath '$ORIGIN' "${filePath}"`, { stdio: 'pipe' });
        logger.debug(`Set RUNPATH for ${file}`);
      } catch (e) {
        logger.log(`Failed to set RUNPATH for ${file}`, 'warning');
      }
    }

    logger.log('Set RUNPATH=$ORIGIN for all binaries', 'success');
  }

  verify() {
    try {
      const output = execSync(`ldd "${this.nodeBin}"`, { encoding: 'utf8' });
      const missing = output.match(/not found/g);

      if (missing) {
        logger.log(`${missing.length} dependencies missing`, 'error');
      } else {
        logger.log('All dependencies resolved', 'success');
      }
    } catch (e) {
      logger.log(`Verification failed: ${e.message}`, 'warning');
    }
  }
}

/**
 * Main function
 */
async function main() {
  const startTime = Date.now();

  logger.start();
  logger.step('Preparing', 1, 3);

  try {
    const { dir, nodeBin } = getTargetDir();
    logger.log(`Target directory: ${dir}`, 'info');
    logger.log(`Binary: ${path.basename(nodeBin)}`, 'info');

    logger.step('Bundling dependencies', 2, 3);

    let bundler;
    switch (process.platform) {
      case 'win32':
        bundler = new WindowsBundler(dir, nodeBin);
        break;
      case 'darwin':
        bundler = new MacOSBundler(dir, nodeBin);
        break;
      case 'linux':
        bundler = new LinuxBundler(dir, nodeBin);
        break;
      default:
        throw new Error(`Unsupported platform: ${process.platform}`);
    }

    await bundler.bundle();

    logger.step('Complete', 3, 3);

    const duration = (Date.now() - startTime) / 1000;
    logger.success(duration);

  } catch (error) {
    const solutions = [];

    if (error.message.includes('not found')) {
      if (process.platform === 'darwin') {
        solutions.push('Install dylibbundler: brew install dylibbundler');
        solutions.push('Install Hamlib: brew install hamlib');
      } else if (process.platform === 'linux') {
        solutions.push('Install patchelf: sudo apt install patchelf');
        solutions.push('Install Hamlib: sudo apt install libhamlib-dev');
      } else if (process.platform === 'win32') {
        solutions.push('Set HAMLIB_ROOT environment variable');
        solutions.push('Install Hamlib to standard location');
      }
    }

    if (error.message.includes('Target directory not found')) {
      solutions.push('Run prebuildify first: npm run prebuild');
    }

    logger.error('Dependency bundling failed', error.message, solutions);
    process.exit(1);
  }
}

if (require.main === module) {
  main().catch(err => {
    console.error('Unexpected error:', err);
    process.exit(1);
  });
}

module.exports = { WindowsBundler, MacOSBundler, LinuxBundler };
