# 从源码构建 Hamlib

本指南介绍如何在各个平台上从源码构建最新版本的 Hamlib 库，并使用它来构建 node-hamlib。

## 目录

- [为什么从源码构建](#为什么从源码构建)
- [快速开始](#快速开始)
- [前置依赖](#前置依赖)
- [使用自动化构建脚本](#使用自动化构建脚本)
- [手动构建步骤](#手动构建步骤)
- [验证安装](#验证安装)
- [常见问题](#常见问题)
- [平台特定说明](#平台特定说明)

---

## 为什么从源码构建

从 Hamlib 源码构建有以下优势：

### ✅ 优势

1. **获取最新功能** - 使用最新的 Hamlib 开发版本，包含最新的设备支持和 bug 修复
2. **版本控制** - 精确控制使用的 Hamlib 版本，而不依赖系统包管理器
3. **优化构建** - 可以根据需要定制构建选项，减少不必要的依赖
4. **跨平台一致性** - 在所有平台使用相同的构建流程，避免系统包版本差异
5. **开发和调试** - 方便修改和调试 Hamlib 源码

### ⚠️ 注意事项

1. **构建时间** - 从源码编译需要 5-10 分钟（取决于 CPU）
2. **磁盘空间** - 需要约 200-300MB 的临时空间用于编译
3. **依赖工具** - 需要安装编译工具链（autotools, gcc/clang）

---

## 快速开始

最简单的方法是使用我们提供的自动化构建脚本：

```bash
# 使用默认设置构建（推荐）
node scripts/build-hamlib.js

# 最小化构建（减少依赖和构建时间）
node scripts/build-hamlib.js --minimal

# 安装到自定义目录（无需 sudo）
node scripts/build-hamlib.js --prefix=$HOME/local/hamlib --minimal

# 查看帮助
node scripts/build-hamlib.js --help
```

---

## 前置依赖

### Linux (Ubuntu/Debian)

```bash
# 基础编译工具
sudo apt-get update
sudo apt-get install -y autoconf automake libtool pkg-config make gcc g++

# 可选：USB 设备支持
sudo apt-get install -y libusb-1.0-0-dev

# 可选：其他功能
sudo apt-get install -y libxml2-dev libreadline-dev
```

### Linux (CentOS/RHEL/Fedora)

```bash
# 基础编译工具
sudo yum install -y autoconf automake libtool pkgconfig make gcc gcc-c++

# 可选：USB 设备支持
sudo yum install -y libusb1-devel

# 可选：其他功能
sudo yum install -y libxml2-devel readline-devel
```

### macOS

```bash
# 安装 Homebrew（如果尚未安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装编译工具
brew install autoconf automake libtool

# 可选：USB 设备支持
brew install libusb
```

### Windows

**推荐：使用官方预编译包**

Windows 平台从源码构建 Hamlib 比较复杂，需要完整的 MSYS2/MinGW 环境。对于大多数用户，我们推荐使用官方预编译包：

下载地址：https://github.com/Hamlib/Hamlib/releases

如果确实需要从源码构建，请参考 [Hamlib 官方文档](https://github.com/Hamlib/Hamlib/blob/master/README.windows.md)。

---

## 使用自动化构建脚本

我们提供了 `scripts/build-hamlib.js` 来自动化整个构建流程。

### 基本用法

```bash
# 1. 安装到项目本地目录（CI 默认行为）
node scripts/build-hamlib.js --prefix=$PWD/hamlib-build --minimal

# 2. 设置环境变量
export HAMLIB_PREFIX=$PWD/hamlib-build
export LD_LIBRARY_PATH=$PWD/hamlib-build/lib:$LD_LIBRARY_PATH  # Linux
# 或
export DYLD_LIBRARY_PATH=$PWD/hamlib-build/lib:$DYLD_LIBRARY_PATH  # macOS

# 3. 构建 node-hamlib
npm run rebuild

# 4. 运行测试
npm test
```

### 安装到系统目录

```bash
# 安装到 /usr/local（需要 sudo）
node scripts/build-hamlib.js --prefix=/usr/local

# 安装到用户目录（不需要 sudo）
node scripts/build-hamlib.js --prefix=$HOME/local/hamlib

# 构建 node-hamlib 时指定路径
export HAMLIB_PREFIX=$HOME/local/hamlib
npm run rebuild
```

### 脚本选项

| 选项 | 说明 |
|------|------|
| `--prefix=<path>` | 指定安装路径。CI 环境默认为 `./hamlib-build`，手动构建默认为 `/usr/local` |
| `--minimal` | 最小化构建，禁用不必要的功能（如 XML、readline、语言绑定等），减少依赖和构建时间 |
| `--help` | 显示帮助信息 |

### 构建流程说明

自动化脚本会执行以下步骤：

1. **检查构建工具** - 验证 git, autoconf, automake, libtool, make 是否安装
2. **克隆源码** - 从 GitHub 克隆最新的 Hamlib 源码
3. **生成配置** - 运行 `./bootstrap` 生成 configure 脚本
4. **配置构建** - 运行 `./configure` 设置安装路径和构建选项
5. **编译** - 并行编译（使用所有 CPU 核心）
6. **安装** - 安装到指定目录
7. **验证** - 检查安装的头文件和库文件

---

## 手动构建步骤

如果你更喜欢手动控制构建流程，可以按照以下步骤操作。

### 步骤 1: 克隆 Hamlib 源码

```bash
git clone --depth 1 https://github.com/Hamlib/Hamlib.git
cd Hamlib
```

**注意**：使用 `--depth 1` 可以只克隆最新提交，减少下载时间和磁盘占用。

### 步骤 2: 生成构建配置

```bash
./bootstrap
```

这一步会使用 autotools 生成 `configure` 脚本。

### 步骤 3: 配置构建选项

#### 基本配置（推荐）

```bash
./configure \
  --prefix=/usr/local \
  --enable-shared=yes \
  --enable-static=no
```

#### 最小化配置（减少依赖）

```bash
./configure \
  --prefix=/usr/local \
  --enable-shared=yes \
  --enable-static=no \
  --disable-winradio \
  --without-cxx-binding \
  --without-perl-binding \
  --without-python-binding \
  --without-tcl-binding \
  --without-lua-binding \
  --without-readline \
  --without-xml-support
```

#### 安装到用户目录（无需 sudo）

```bash
./configure \
  --prefix=$HOME/local/hamlib \
  --enable-shared=yes \
  --enable-static=no \
  --without-cxx-binding \
  --without-perl-binding \
  --without-python-binding \
  --without-readline \
  --without-xml-support
```

### 步骤 4: 编译

```bash
# 并行编译（使用所有 CPU 核心）
make -j$(nproc)  # Linux
make -j$(sysctl -n hw.ncpu)  # macOS

# 可选：运行测试
make check
```

### 步骤 5: 安装

```bash
# 安装到系统目录（需要 sudo）
sudo make install

# 或安装到用户目录（不需要 sudo）
make install
```

### 步骤 6: 更新库缓存（仅 Linux）

```bash
# 如果安装到 /usr/local
sudo ldconfig
```

---

## 验证安装

### 检查安装的文件

```bash
# 检查头文件
ls -la /usr/local/include/hamlib/

# 检查库文件（Linux）
ls -la /usr/local/lib/libhamlib*

# 检查库文件（macOS）
ls -la /usr/local/lib/libhamlib*.dylib

# 检查工具
which rigctl rigctld
```

### 测试 Hamlib 工具

```bash
# 查看版本
rigctl --version

# 列出支持的设备
rigctl -l

# 测试虚拟设备
rigctl -m 1 -r /dev/ttyUSB0
```

### 使用 pkg-config 验证

```bash
# 查看安装信息
pkg-config --modversion hamlib
pkg-config --cflags hamlib
pkg-config --libs hamlib
```

---

## 常见问题

### Q1: 提示缺少 autoconf/automake/libtool

**问题**：运行 `./bootstrap` 时报错找不到工具。

**解决方案**：

```bash
# Linux (Ubuntu/Debian)
sudo apt-get install autoconf automake libtool

# Linux (CentOS/RHEL)
sudo yum install autoconf automake libtool

# macOS
brew install autoconf automake libtool
```

### Q2: configure 时提示缺少依赖

**问题**：`./configure` 报错缺少某些库。

**解决方案**：

方案 1（推荐）：使用 `--minimal` 禁用不必要的功能：

```bash
node scripts/build-hamlib.js --minimal
```

方案 2：安装缺少的依赖库。

### Q3: 编译时内存不足

**问题**：系统内存小于 1GB 时，并行编译可能导致内存不足。

**解决方案**：

减少并行编译的线程数：

```bash
make -j2  # 只使用 2 个线程
make      # 串行编译（不使用并行）
```

### Q4: node-hamlib 找不到 Hamlib 库

**问题**：构建 node-hamlib 时报错找不到 hamlib 头文件或库。

**解决方案**：

设置 `HAMLIB_PREFIX` 环境变量：

```bash
export HAMLIB_PREFIX=/path/to/hamlib/install
export PKG_CONFIG_PATH=$HAMLIB_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH

# Linux
export LD_LIBRARY_PATH=$HAMLIB_PREFIX/lib:$LD_LIBRARY_PATH

# macOS
export DYLD_LIBRARY_PATH=$HAMLIB_PREFIX/lib:$DYLD_LIBRARY_PATH

npm run rebuild
```

### Q5: 运行时找不到动态库

**问题**：运行程序时报错找不到 `libhamlib.so` 或 `libhamlib.dylib`。

**解决方案**：

**Linux**：

```bash
# 方案 1：设置 LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/path/to/hamlib/lib:$LD_LIBRARY_PATH

# 方案 2：添加到系统库路径
echo "/path/to/hamlib/lib" | sudo tee /etc/ld.so.conf.d/hamlib.conf
sudo ldconfig

# 方案 3：使用 RPATH（编译时设置）
# 已在 binding.gyp 中配置
```

**macOS**：

```bash
# 方案 1：设置 DYLD_LIBRARY_PATH
export DYLD_LIBRARY_PATH=/path/to/hamlib/lib:$DYLD_LIBRARY_PATH

# 方案 2：使用 install_name_tool（推荐）
# 已由 bundle-macos.sh 自动处理
```

### Q6: Windows 构建失败

**问题**：在 Windows 上构建遇到各种错误。

**解决方案**：

Windows 从源码构建比较复杂。推荐方案：

1. 使用官方预编译包（推荐）
2. 使用 WSL（Windows Subsystem for Linux）按照 Linux 步骤构建
3. 安装完整的 MSYS2 环境

### Q7: CI 构建时间过长

**问题**：GitHub Actions 或其他 CI 环境中构建时间超过 10 分钟。

**解决方案**：

1. 使用 `--minimal` 选项减少构建时间
2. 缓存构建结果
3. 考虑使用预编译包代替源码构建

---

## 平台特定说明

### Linux

#### Ubuntu/Debian

```bash
# 完整步骤
sudo apt-get update
sudo apt-get install -y autoconf automake libtool pkg-config make gcc g++
node scripts/build-hamlib.js --prefix=$HOME/local/hamlib --minimal
export HAMLIB_PREFIX=$HOME/local/hamlib
export LD_LIBRARY_PATH=$HAMLIB_PREFIX/lib:$LD_LIBRARY_PATH
npm run rebuild
```

#### CentOS/RHEL

```bash
# 完整步骤
sudo yum install -y autoconf automake libtool pkgconfig make gcc gcc-c++
node scripts/build-hamlib.js --prefix=$HOME/local/hamlib --minimal
export HAMLIB_PREFIX=$HOME/local/hamlib
export LD_LIBRARY_PATH=$HAMLIB_PREFIX/lib:$LD_LIBRARY_PATH
npm run rebuild
```

#### 架构支持

- **x86_64** - 完全支持
- **aarch64 (ARM64)** - 完全支持
- **armv7l (ARM32)** - 支持，但需要确保有足够内存编译

### macOS

#### Intel Mac

Homebrew 安装在 `/usr/local`：

```bash
brew install autoconf automake libtool
node scripts/build-hamlib.js --prefix=$HOME/local/hamlib --minimal
export HAMLIB_PREFIX=$HOME/local/hamlib
npm run rebuild
```

#### Apple Silicon (M1/M2/M3)

Homebrew 安装在 `/opt/homebrew`：

```bash
brew install autoconf automake libtool
node scripts/build-hamlib.js --prefix=$HOME/local/hamlib --minimal
export HAMLIB_PREFIX=$HOME/local/hamlib
npm run rebuild
```

#### 注意事项

1. **Xcode Command Line Tools** - 确保已安装：`xcode-select --install`
2. **Rosetta** - Apple Silicon 可以运行 x86_64 和 arm64 两种架构
3. **System Integrity Protection (SIP)** - 可能阻止修改系统目录，建议安装到用户目录

### Windows

#### 推荐方案：使用预编译包

```powershell
# 下载官方预编译包
# https://github.com/Hamlib/Hamlib/releases

# 解压到 C:\hamlib
# 设置环境变量
$env:HAMLIB_ROOT = "C:\hamlib"

# 构建 node-hamlib
npm run rebuild
```

#### 高级方案：使用 WSL2

在 Windows 11 或 Windows 10 (2004+) 上使用 WSL2：

```bash
# 在 WSL2 中按照 Linux 步骤操作
wsl --install -d Ubuntu
wsl

# 进入 WSL2 后
cd /mnt/c/your/project
sudo apt-get update
sudo apt-get install -y autoconf automake libtool
node scripts/build-hamlib.js --minimal
npm run rebuild
```

#### 高级方案：使用 MSYS2

如果必须在 Windows 原生环境中构建：

1. 安装 [MSYS2](https://www.msys2.org/)
2. 在 MSYS2 终端中安装工具链：
   ```bash
   pacman -S base-devel mingw-w64-x86_64-toolchain autoconf automake libtool
   ```
3. 克隆 Hamlib 并构建
4. 设置 `HAMLIB_ROOT` 环境变量

---

## 进阶配置

### 使用特定版本的 Hamlib

如果需要使用特定版本而不是最新版本：

```bash
git clone https://github.com/Hamlib/Hamlib.git
cd Hamlib
git checkout 4.6.5  # 切换到特定标签或分支
./bootstrap
./configure --prefix=/usr/local --minimal
make -j$(nproc)
sudo make install
```

### 启用 USB 设备支持

```bash
# Linux: 安装 libusb
sudo apt-get install -y libusb-1.0-0-dev

# macOS: 安装 libusb
brew install libusb

# 构建时会自动检测并启用
./configure --prefix=/usr/local --with-libusb=yes
```

### 启用 XML 支持

```bash
# Linux
sudo apt-get install -y libxml2-dev

# macOS
brew install libxml2

# 构建
./configure --prefix=/usr/local --with-xml-support=yes
```

### 静态链接

如果需要静态链接 Hamlib（减少运行时依赖）：

```bash
./configure \
  --prefix=/usr/local \
  --enable-shared=no \
  --enable-static=yes
```

**注意**：node-hamlib 默认配置为动态链接。

---

## 清理和卸载

### 清理构建文件

```bash
cd Hamlib
make clean      # 清理编译产物
make distclean  # 清理所有生成文件
```

### 卸载 Hamlib

```bash
cd Hamlib
sudo make uninstall

# 手动清理（如果 make uninstall 不可用）
sudo rm -rf /usr/local/include/hamlib
sudo rm -f /usr/local/lib/libhamlib*
sudo rm -f /usr/local/bin/rigctl*
sudo rm -f /usr/local/bin/rotctl*
```

---

## 参考资源

- [Hamlib 官方网站](https://hamlib.github.io/)
- [Hamlib GitHub 仓库](https://github.com/Hamlib/Hamlib)
- [Hamlib 构建文档](https://github.com/Hamlib/Hamlib/blob/master/README)
- [node-hamlib 项目](https://github.com/your-repo/node-hamlib)

---

## 贡献和反馈

如果在构建过程中遇到问题或有改进建议，欢迎：

1. 提交 [GitHub Issue](https://github.com/your-repo/node-hamlib/issues)
2. 参与 [Discussions](https://github.com/your-repo/node-hamlib/discussions)
3. 提交 Pull Request

---

**最后更新**: 2024-11-07

如有问题，请参考 [故障排除指南](troubleshooting.md) 或在项目 Issues 中提问。
