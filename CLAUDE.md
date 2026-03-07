# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

node-hamlib 是 Hamlib 业余无线电控制库的 Node.js 绑定。通过纯 C shim 层 + N-API C++ addon，提供异步 Promise-based API 控制业余无线电设备。支持 5 个平台的预编译二进制分发。

## Architecture

```
JavaScript (lib/index.js)
    ↓ async wrapper
C++ N-API addon (src/hamlib.cpp, MSVC on Windows)
    ↓ shim_rig_*() calls
Pure C shim (src/shim/hamlib_shim.c, MinGW DLL on Windows / static .a on Unix)
    ↓ rig_*() calls
libhamlib
```

### Shim Layer（关键架构）

shim 层解决 Windows 上 MSVC addon 与 MinGW Hamlib DLL 的跨编译器 ABI 不兼容问题（struct 内存布局差异导致崩溃）。所有平台统一使用 shim，保持代码一致性。

- `src/shim/hamlib_shim.h` — 纯 C 头文件，定义 `hamlib_shim_handle_t`（不透明 `void*` 句柄）和 ~95 个 `shim_rig_*` 函数
- `src/shim/hamlib_shim.c` — 实现，内部 cast 回 `RIG*` 并调用 Hamlib API
- `scripts/build-shim.js` — 编译脚本。Linux/macOS: `gcc → .a`（静态链入 addon）；Windows: `MinGW gcc → .dll`（addon 通过 MSVC import lib 动态加载）
- shim 输出到 `shim-build/` 目录（避免被 node-gyp rebuild 清除 `build/`）

**hamlib.cpp 中无条件编译**，统一通过 `shim_rig_*()` 调用 Hamlib，不直接 `#include <hamlib/rig.h>`，不直接访问 `RIG` 结构体。

### Key Files

| 文件 | 说明 |
|------|------|
| `src/shim/hamlib_shim.h/.c` | 纯 C shim 层（~95 个函数） |
| `src/hamlib.cpp` | C++ N-API addon 主实现（~5300 行） |
| `src/hamlib.h` | C++ 头文件，使用 `void*` 不透明句柄 |
| `src/addon.cpp` | addon 入口 |
| `lib/index.js` | JavaScript 包装层 |
| `index.d.ts` | TypeScript 类型定义 |
| `binding.gyp` | 构建配置，链接 `shim-build/` 中的 shim 库 |
| `scripts/build-shim.js` | shim 编译脚本 |
| `scripts/build-all.js` | 统一构建脚本 |
| `scripts/bundle-deps.js` | 运行时依赖打包 |

## Commands

```bash
npm run build:all              # 完整构建（shim + addon + prebuild + bundle）
npm run build:all -- --minimal # 最小化 Hamlib 构建
npm test                       # test_loader.js
node test/test_ci_functional.js # 功能测试（Dummy 设备，无需硬件）
node test/test_loader.js       # API 存在性测试
```

## Adding a New API Method

四层必须同步修改。以添加 `setNewFeature(param)` 为例：

### Step 1: Shim 层

`src/shim/hamlib_shim.h`:
```c
SHIM_API int shim_rig_set_new_feature(hamlib_shim_handle_t h, int param);
```

`src/shim/hamlib_shim.c`:
```c
int shim_rig_set_new_feature(hamlib_shim_handle_t h, int param) {
    RIG *rig = (RIG *)h;
    if (!rig) return -RIG_EINVAL;
    return rig_set_new_feature(rig, param);
}
```

### Step 2: C++ addon

`src/hamlib.cpp`:
1. 创建 `SetNewFeatureAsyncWorker` 类
2. 实现 `SetNewFeature` 方法，调用 `shim_rig_set_new_feature()`
3. 在 `DefineClass` 中注册: `InstanceMethod("setNewFeature", &NodeHamLib::SetNewFeature)`

### Step 3: JavaScript 包装

`lib/index.js`:
```javascript
async setNewFeature(param) {
  return this._nativeInstance.setNewFeature(param);
}
```

### Step 4: TypeScript 定义

`index.d.ts`:
```typescript
setNewFeature(param: number): Promise<number>;
```

### 验证

```bash
node test/test_loader.js         # 方法存在性
node test/test_ci_functional.js  # 功能验证
```

## Testing

CI 运行两个测试，均不需要硬件：
- `test_loader.js` — 模块加载 + 84 个方法存在性检查
- `test_ci_functional.js` — Dummy 设备核心 API 功能验证（31 项）

其他测试：
- `test_serial_config.js` — 串口配置（Dummy 设备，无需硬件）
- `test_dummy_complete.js` — 完整 API 测试（需 `rigctld -m 1 -t 4532`）
- `test_network.js` — 网络连接测试（需 rigctld + 硬件）

## Publishing

```bash
npm version patch && git push && git push --tags
```

Tag push 触发 CI 自动：构建 5 平台 → 测试 → 验证 → npm publish → GitHub Release。
需要 GitHub Secret: `NPM_TOKEN`。详见 `PUBLISHING.md`。

## Historical Lessons

### Split API 参数顺序（已修复）

曾因注释与实现不一致导致 `setSplit(enable, rxVfo, txVfo)` 的参数被错误交换。教训：**修改涉及参数顺序的 API 时，必须检查 C++ 实现中参数的实际处理逻辑**，不能只看注释。三层 API（JS/C++/TS）参数顺序必须完全一致。
