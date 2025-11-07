#!/usr/bin/env node

/**
 * Hamlib 统一构建脚本
 * 从GitHub源码构建最新版本的Hamlib库
 *
 * 使用方法:
 *   node scripts/build-hamlib.js [选项]
 *
 * 选项:
 *   --prefix=<path>   自定义安装路径 (默认: CI环境用./hamlib-build, 手动用/usr/local)
 *   --minimal         最小化构建，减少依赖和构建时间
 *   --help           显示帮助信息
 */

const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

// 颜色输出
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  red: '\x1b[31m',
  cyan: '\x1b[36m',
};

function log(message, color = 'reset') {
  console.log(`${colors[color]}${message}${colors.reset}`);
}

function logStep(step, message) {
  log(`\n[${step}] ${message}`, 'cyan');
}

function logSuccess(message) {
  log(`✓ ${message}`, 'green');
}

function logWarning(message) {
  log(`⚠ ${message}`, 'yellow');
}

function logError(message) {
  log(`✗ ${message}`, 'red');
}

function exec(command, options = {}) {
  try {
    return execSync(command, {
      stdio: options.silent ? 'pipe' : 'inherit',
      encoding: 'utf-8',
      shell: true,  // 确保shell命令和重定向正常工作
      ...options
    });
  } catch (error) {
    if (options.ignoreError) {
      return null;
    }
    throw error;
  }
}

// 解析命令行参数
function parseArgs() {
  const args = process.argv.slice(2);
  const config = {
    prefix: null,
    minimal: false,
    help: false,
  };

  for (const arg of args) {
    if (arg.startsWith('--prefix=')) {
      config.prefix = arg.substring(9);
    } else if (arg === '--minimal') {
      config.minimal = true;
    } else if (arg === '--help' || arg === '-h') {
      config.help = true;
    } else {
      logWarning(`未知参数: ${arg}`);
    }
  }

  return config;
}

function showHelp() {
  console.log(`
${colors.bright}Hamlib 源码构建脚本${colors.reset}

${colors.cyan}使用方法:${colors.reset}
  node scripts/build-hamlib.js [选项]

${colors.cyan}选项:${colors.reset}
  --prefix=<path>   自定义安装路径
                    默认: CI环境用 ./hamlib-build
                         手动构建用 /usr/local (需sudo)

  --minimal         最小化构建，禁用不必要的功能
                    减少编译依赖和构建时间

  --help, -h        显示此帮助信息

${colors.cyan}示例:${colors.reset}
  # CI环境（自动检测）
  node scripts/build-hamlib.js --minimal

  # 安装到用户目录（无需sudo）
  node scripts/build-hamlib.js --prefix=$HOME/local/hamlib

  # 安装到系统路径（需要sudo）
  node scripts/build-hamlib.js --prefix=/usr/local

${colors.cyan}注意事项:${colors.reset}
  - Linux/macOS: 需要 autoconf, automake, libtool
  - Windows: 此脚本会跳过，请使用官方预编译包
  - 构建时间: 约5-10分钟（取决于CPU和配置）
`);
}

// 检测平台
function detectPlatform() {
  const platform = os.platform();
  if (platform === 'win32') {
    return 'windows';
  } else if (platform === 'darwin') {
    return 'macos';
  } else if (platform === 'linux') {
    return 'linux';
  } else {
    return 'unknown';
  }
}

// 检测是否在CI环境
function isCI() {
  return !!(process.env.CI || process.env.GITHUB_ACTIONS);
}

// 检查必需的构建工具
function checkBuildTools(platform) {
  logStep('1/6', '检查构建工具');

  const requiredTools = ['git', 'autoconf', 'automake', 'libtoolize', 'make'];
  const missingTools = [];

  for (const tool of requiredTools) {
    try {
      // 简化：只检测工具是否可执行，不获取详细路径
      exec(`${tool} --version 2>&1`, { silent: true, ignoreError: false });
      logSuccess(`找到 ${tool}`);
    } catch (error) {
      missingTools.push(tool);
    }
  }

  if (missingTools.length > 0) {
    logError(`缺少必需的构建工具: ${missingTools.join(', ')}`);
    const installCmd = platform === 'macos'
      ? 'brew install autoconf automake libtool'
      : 'sudo apt-get install autoconf automake libtool pkg-config';
    log(`安装命令: ${installCmd}`, 'bright');
    process.exit(1);
  }

  logSuccess('所有必需工具已安装');
}

// 克隆Hamlib仓库
function cloneHamlib(workDir) {
  logStep('2/6', '克隆 Hamlib 源码');

  const hamlibDir = path.join(workDir, 'Hamlib');

  if (fs.existsSync(hamlibDir)) {
    logWarning('Hamlib 目录已存在，删除旧版本...');
    exec(`rm -rf "${hamlibDir}"`);
  }

  log('从 GitHub 克隆最新源码...', 'blue');
  exec(`git clone --depth 1 https://github.com/Hamlib/Hamlib.git "${hamlibDir}"`);

  logSuccess('源码克隆完成');

  return hamlibDir;
}

// 构建Hamlib
function buildHamlib(hamlibDir, prefix, minimal) {
  logStep('3/6', '生成构建配置');

  process.chdir(hamlibDir);

  // 运行bootstrap生成configure脚本
  log('运行 ./bootstrap ...', 'blue');
  exec('./bootstrap');
  logSuccess('构建配置已生成');

  // 配置构建选项
  logStep('4/6', '配置构建选项');

  let configureOptions = [
    `--prefix="${prefix}"`,
    '--enable-shared=yes',
    '--enable-static=no',
  ];

  if (minimal) {
    log('使用最小化构建配置', 'yellow');
    configureOptions = configureOptions.concat([
      '--disable-winradio',
      '--without-cxx-binding',
      '--without-perl-binding',
      '--without-python-binding',
      '--without-tcl-binding',
      '--without-lua-binding',
      '--without-readline',
      '--without-xml-support',
    ]);
  }

  const configureCmd = `./configure ${configureOptions.join(' ')}`;
  log('配置命令: ' + configureCmd, 'blue');
  exec(configureCmd);
  logSuccess('配置完成');

  // 编译
  logStep('5/6', '编译 Hamlib');

  const cores = os.cpus().length;
  log(`使用 ${cores} 个CPU核心并行编译...`, 'blue');
  exec(`make -j${cores}`);
  logSuccess('编译完成');

  // 安装
  logStep('6/6', '安装 Hamlib');

  // 检查是否需要sudo
  const needsSudo = !fs.existsSync(prefix) && prefix.startsWith('/usr');
  const installCmd = needsSudo ? 'sudo make install' : 'make install';

  if (needsSudo) {
    logWarning('安装到系统路径需要 sudo 权限');
  }

  log(`运行: ${installCmd}`, 'blue');
  exec(installCmd);
  logSuccess('安装完成');
}

// 验证安装
function verifyInstallation(prefix, platform) {
  log('\n' + '='.repeat(60), 'green');
  log('验证安装', 'green');
  log('='.repeat(60), 'green');

  const includeDir = path.join(prefix, 'include', 'hamlib');
  const libDir = path.join(prefix, 'lib');

  // 检查头文件
  if (fs.existsSync(includeDir)) {
    logSuccess(`头文件目录: ${includeDir}`);
    const headers = fs.readdirSync(includeDir).filter(f => f.endsWith('.h'));
    log(`  找到 ${headers.length} 个头文件`, 'blue');
  } else {
    logError(`头文件目录不存在: ${includeDir}`);
  }

  // 检查库文件
  if (fs.existsSync(libDir)) {
    logSuccess(`库文件目录: ${libDir}`);
    const libs = fs.readdirSync(libDir);

    let foundLib = false;
    if (platform === 'macos') {
      const dylibFiles = libs.filter(f => f.includes('libhamlib') && f.endsWith('.dylib'));
      if (dylibFiles.length > 0) {
        dylibFiles.forEach(f => log(`  ${f}`, 'blue'));
        foundLib = true;
      }
    } else if (platform === 'linux') {
      const soFiles = libs.filter(f => f.includes('libhamlib') && f.includes('.so'));
      if (soFiles.length > 0) {
        soFiles.forEach(f => log(`  ${f}`, 'blue'));
        foundLib = true;
      }
    }

    if (!foundLib) {
      logWarning('未找到 Hamlib 动态库文件');
    }
  } else {
    logError(`库文件目录不存在: ${libDir}`);
  }

  // 输出环境变量设置提示
  log('\n' + '='.repeat(60), 'cyan');
  log('环境变量配置', 'cyan');
  log('='.repeat(60), 'cyan');

  console.log('\n构建 node-hamlib 时，请设置以下环境变量:\n');
  log(`export HAMLIB_PREFIX="${prefix}"`, 'bright');

  if (platform === 'linux') {
    log(`export LD_LIBRARY_PATH="${libDir}:$LD_LIBRARY_PATH"`, 'bright');
  } else if (platform === 'macos') {
    log(`export DYLD_LIBRARY_PATH="${libDir}:$DYLD_LIBRARY_PATH"`, 'bright');
  }

  log(`export PKG_CONFIG_PATH="${path.join(libDir, 'pkgconfig')}:$PKG_CONFIG_PATH"`, 'bright');

  console.log('\n然后运行:\n');
  log('npm run rebuild', 'bright');
  console.log();
}

// 主函数
async function main() {
  const config = parseArgs();

  if (config.help) {
    showHelp();
    process.exit(0);
  }

  log('\n' + '='.repeat(60), 'bright');
  log('Hamlib 源码构建脚本', 'bright');
  log('='.repeat(60) + '\n', 'bright');

  const platform = detectPlatform();
  const ci = isCI();

  log(`平台: ${platform}`, 'blue');
  log(`CI环境: ${ci ? '是' : '否'}`, 'blue');

  // Windows平台跳过
  if (platform === 'windows') {
    logWarning('Windows 平台不支持源码构建');
    log('请使用官方预编译包: https://github.com/Hamlib/Hamlib/releases', 'yellow');
    process.exit(0);
  }

  // 确定安装路径
  let prefix = config.prefix;
  if (!prefix) {
    if (ci) {
      prefix = path.resolve(process.cwd(), 'hamlib-build');
      log(`CI环境，使用本地路径: ${prefix}`, 'yellow');
    } else {
      prefix = '/usr/local';
      log(`默认安装路径: ${prefix}`, 'yellow');
    }
  }

  log(`安装路径: ${prefix}\n`, 'green');

  // 检查构建工具
  checkBuildTools(platform);

  // 创建工作目录
  const workDir = path.join(os.tmpdir(), `hamlib-build-${Date.now()}`);
  fs.mkdirSync(workDir, { recursive: true });
  log(`工作目录: ${workDir}\n`, 'blue');

  try {
    // 克隆仓库
    const hamlibDir = cloneHamlib(workDir);

    // 构建
    buildHamlib(hamlibDir, prefix, config.minimal);

    // 验证
    verifyInstallation(prefix, platform);

    // 清理工作目录
    log('\n清理临时文件...', 'blue');
    exec(`rm -rf "${workDir}"`);

    log('\n' + '='.repeat(60), 'green');
    log('构建成功完成！', 'green');
    log('='.repeat(60) + '\n', 'green');

  } catch (error) {
    logError('\n构建失败: ' + error.message);

    // 清理工作目录
    if (fs.existsSync(workDir)) {
      exec(`rm -rf "${workDir}"`, { ignoreError: true });
    }

    process.exit(1);
  }
}

// 运行主函数
main().catch(error => {
  logError('发生错误: ' + error.message);
  process.exit(1);
});
