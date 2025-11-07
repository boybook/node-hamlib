#!/usr/bin/env node

/**
 * 环境变量 wrapper - 自动设置 Hamlib 环境变量后运行命令
 * 用法: node scripts/run-with-env.js <command>
 */

const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const rootDir = path.resolve(__dirname, '..');
const hamlibBuildDir = path.join(rootDir, 'hamlib-build');

// 检查是否存在本地编译的 Hamlib
if (fs.existsSync(hamlibBuildDir)) {
  const libDir = path.join(hamlibBuildDir, 'lib');
  const pkgConfigDir = path.join(libDir, 'pkgconfig');

  // 设置环境变量
  process.env.HAMLIB_PREFIX = hamlibBuildDir;
  process.env.LD_LIBRARY_PATH = `${libDir}:${process.env.LD_LIBRARY_PATH || ''}`;
  process.env.DYLD_LIBRARY_PATH = `${libDir}:${process.env.DYLD_LIBRARY_PATH || ''}`;
  process.env.PKG_CONFIG_PATH = `${pkgConfigDir}:${process.env.PKG_CONFIG_PATH || ''}`;

  console.log(`✓ 已设置 Hamlib 环境变量: ${hamlibBuildDir}`);
}

// 获取要执行的命令
const command = process.argv.slice(2).join(' ');

if (!command) {
  console.error('错误: 请提供要执行的命令');
  console.error('用法: node scripts/run-with-env.js <command>');
  process.exit(1);
}

// 执行命令
try {
  execSync(command, {
    stdio: 'inherit',
    cwd: rootDir,
    shell: true
  });
} catch (error) {
  process.exit(error.status || 1);
}
