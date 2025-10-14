#!/usr/bin/env node

/**
 * 验证构建产物的完整性和正确性
 * 用于GitHub Actions构建流程中的质量检查
 */

const fs = require('fs');
const path = require('path');

console.log('🔍 Node-HamLib Build Verification');
console.log('=================================');

// 预期的平台基名（允许 libc 标签后缀，如 +glibc/+musl）
const expectedPlatforms = [
  'linux-x64',
  'linux-arm64',
  'darwin-arm64'
  // 'win32-x64'  // 暂不启用 Windows
];

const prebuildsDir = path.join(process.cwd(), 'prebuilds');

// 验证 prebuilds 目录存在
if (!fs.existsSync(prebuildsDir)) {
  console.error('❌ Error: prebuilds directory not found');
  process.exit(1);
}

console.log('✅ prebuilds directory exists');

// 检查每个平台目录
let foundPlatforms = 0;
let totalBinaries = 0;
let totalSize = 0;

// 递归查找包含 node.napi.node 的目录
function hasNapiBinary(dir) {
  try {
    const entries = fs.readdirSync(dir, { withFileTypes: true });
    return entries.some(e => e.isFile() && /^node\.napi(\.[^.]+)?\.node$/.test(e.name));
  } catch (_) {
    return false;
  }
}

function findBinaryDirs(dir) {
  const results = [];
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      if (hasNapiBinary(full)) {
        results.push(full);
      }
      results.push(...findBinaryDirs(full));
    }
  }
  return results;
}

const binaryDirs = Array.from(new Set(findBinaryDirs(prebuildsDir)));

for (const base of expectedPlatforms) {
  const matched = binaryDirs.filter(d => {
    const rel = path.relative(prebuildsDir, d);
    const name = path.basename(d);
    // 允许目录名等于、以 base+ 开头，或者路径中包含 base/ 或 base+
    return (
      name === base ||
      name.startsWith(base + '+') ||
      rel.includes(`${base}/`) ||
      rel.includes(`${base}+`)
    );
  });

  if (matched.length === 0) {
    console.log(`❌ Platform directory missing: ${base} (no matches)`);
    continue;
  }

  const chosen = matched[0];
  // 选择实际存在的 node.napi*.node 文件
  let binaryPath = path.join(chosen, 'node.napi.node');
  if (!fs.existsSync(binaryPath)) {
    const candidates = fs.readdirSync(chosen)
      .filter(n => /^node\.napi(\.[^.]+)?\.node$/.test(n))
      .map(n => path.join(chosen, n));
    if (candidates.length === 0) {
      console.log(`  ❌ node.napi*.node missing in ${path.relative(prebuildsDir, chosen)}`);
      continue;
    }
    binaryPath = candidates[0];
  }
  const stats = fs.statSync(binaryPath);
  console.log(`📁 ${path.relative(prebuildsDir, chosen)}`);
  console.log(`  ✅ Binary found: ${path.basename(binaryPath)} (${stats.size} bytes)`);

  // 检查是否捆绑了 hamlib 动态库
  const files = fs.readdirSync(chosen);
  const hasHamlib = files.some(n => /^libhamlib\.(so(\..*)?|dylib)$/.test(n));
  if (hasHamlib) {
    console.log(`  ✅ Found bundled hamlib runtime library`);
  } else {
    console.log(`  ⚠️  hamlib runtime library not bundled in ${path.relative(prebuildsDir, chosen)}`);
  }
  totalBinaries++;
  totalSize += stats.size;
  foundPlatforms++;
}

// 检查BUILD_INFO.txt
const buildInfoPath = path.join(prebuildsDir, 'BUILD_INFO.txt');
if (fs.existsSync(buildInfoPath)) {
  console.log('✅ BUILD_INFO.txt found');
  const buildInfo = fs.readFileSync(buildInfoPath, 'utf8');
  console.log('📋 Build Information:');
  console.log(buildInfo.split('\n').slice(0, 8).map(line => `  ${line}`).join('\n'));
} else {
  console.log('⚠️  BUILD_INFO.txt not found');
}

// 统计信息
console.log('\n📊 Build Summary');
console.log('================');
console.log(`Platform directories found: ${foundPlatforms}/${expectedPlatforms.length}`);
console.log(`Binary files found: ${totalBinaries}/${expectedPlatforms.length}`);
console.log(`Total binaries size: ${(totalSize / 1024 / 1024).toFixed(2)} MB`);

// 检查是否有统一的ZIP包
const zipPath = path.join(process.cwd(), 'node-hamlib-prebuilds.zip');
if (fs.existsSync(zipPath)) {
  const zipStats = fs.statSync(zipPath);
  console.log(`Package file: node-hamlib-prebuilds.zip (${(zipStats.size / 1024 / 1024).toFixed(2)} MB)`);
  console.log('✅ Unified package created successfully');
} else {
  console.log('⚠️  Unified package not found');
}

// 总体验证结果
if (foundPlatforms === expectedPlatforms.length && totalBinaries === expectedPlatforms.length) {
  console.log('\n🎉 Build verification passed!');
  console.log('All platforms built successfully and binaries are present.');
  process.exit(0);
} else {
  console.log('\n❌ Build verification failed!');
  console.log(`Expected ${expectedPlatforms.length} platforms, found ${foundPlatforms}`);
  console.log(`Expected ${expectedPlatforms.length} binaries, found ${totalBinaries}`);
  process.exit(1);
}
