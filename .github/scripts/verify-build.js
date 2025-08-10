#!/usr/bin/env node

/**
 * 验证构建产物的完整性和正确性
 * 用于GitHub Actions构建流程中的质量检查
 */

const fs = require('fs');
const path = require('path');

console.log('🔍 Node-HamLib Build Verification');
console.log('=================================');

// 检查预期的目录结构
const expectedPlatforms = [
  'linux-x64',
  'linux-arm64', 
  'darwin-arm64'
  // 'win32-x64'  // 暂时注释掉 Windows 支持
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

for (const platform of expectedPlatforms) {
  const platformDir = path.join(prebuildsDir, platform);
  const binaryPath = path.join(platformDir, 'hamlib.node');
  
  if (fs.existsSync(platformDir)) {
    console.log(`📁 Platform directory found: ${platform}`);
    foundPlatforms++;
    
    if (fs.existsSync(binaryPath)) {
      const stats = fs.statSync(binaryPath);
      console.log(`  ✅ Binary found: hamlib.node (${stats.size} bytes)`);
      totalBinaries++;
      totalSize += stats.size;
      
      // 简单的二进制文件验证
      if (stats.size < 100) {
        console.log(`  ⚠️  Warning: Binary size seems too small (${stats.size} bytes)`);
      }
      
    } else {
      console.log(`  ❌ Binary missing: hamlib.node`);
    }
  } else {
    console.log(`❌ Platform directory missing: ${platform}`);
  }
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