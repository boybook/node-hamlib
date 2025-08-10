# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

node-hamlib是一个Node.js绑定，用于Hamlib业余无线电收发器控制库。它通过Node.js Addon API (NAPI)将C++代码与JavaScript接口连接，提供了完整的异步Promise-based API来控制业余无线电设备。

项目目录下提供了原始Hamlib仓库的C++实现（只读），你可以进行参考。

## Development Commands

### Building and Compilation
- `npm run build` - 配置并构建项目（node-gyp configure && node-gyp build）
- `npm run rebuild` - 重新构建项目（node-gyp rebuild）
- `npm run clean` - 清理构建文件（node-gyp clean）

### Testing
- `npm test` - 运行基础加载器测试（test/test_loader.js）
- `npm run test:network` - 运行网络连接测试（test/test_network.js）

### Installation
- `npm install` - 安装依赖并自动运行安装脚本
- 安装脚本：`scripts/install.js` 处理预编译二进制文件的安装和回退到源码构建

## Architecture Overview

### Core Components

**Native C++ Layer (src/):**
- `hamlib.cpp` - 主要的C++实现，包含所有Hamlib操作的异步封装器
- `hamlib.h` - C++头文件，定义NodeHamLib类和AsyncWorker基类
- `decoder.cpp/decoder.h` - 数据解码功能
- `addon.cpp` - Node.js addon入口点

**JavaScript Layer:**
- `index.js` - CommonJS主入口，处理二进制加载
- `lib/index.mjs` - ES模块入口
- `lib/binary-loader.js` - 跨平台预编译二进制加载逻辑

**TypeScript Definitions:**
- `index.d.ts` - 完整的TypeScript类型定义，包含所有API接口和类型

### Promise-Based Async Architecture

所有无线电控制操作都通过AsyncWorker类实现为异步Promise：
- 基类：`HamLibAsyncWorker` 提供Promise支持
- 每个操作都有专门的AsyncWorker子类（如OpenAsyncWorker, SetFrequencyAsyncWorker等）
- 所有API方法返回Promise，支持async/await和.then()/.catch()

### Binary Distribution Strategy

项目支持多平台预编译二进制：
- **支持平台**: darwin-arm64, linux-x64, linux-arm64, windows-x64
- **回退机制**: 如果预编译二进制不可用，自动从源码构建
- **加载逻辑**: `lib/binary-loader.js` 处理平台检测和二进制加载

### Cross-Platform Build Configuration (binding.gyp)

复杂的构建配置支持多个平台和环境：
- **Linux**: 使用系统libhamlib（/usr/include, /usr/local/include）
- **macOS**: 支持Homebrew路径（/opt/homebrew/include）
- **Windows**: 支持MSVC和MinGW环境，包含复杂的路径检测逻辑

## Key API Patterns

### Connection Management
```javascript
const rig = new HamLib(modelNumber, portPath);
await rig.open();  // 异步连接
// ... operations
await rig.close(); // 异步断开
```

### Network vs Serial Connection Detection
代码自动检测连接类型：
- 包含冒号的地址被识别为网络连接（localhost:4532）
- 其他路径被视为串口连接（/dev/ttyUSB0, COM3）

### Error Handling Pattern
所有异步操作遵循相同的错误处理模式：
- 成功：Promise resolve with result code
- 失败：Promise reject with error message from rigerror()

## Testing Strategy

### 优化后的测试结构 (8个核心测试)
项目测试已优化，从15个文件减少到8个，消除重复并提升质量：

**核心测试 (无需硬件)**:
- `test_loader.js` - 基础模块加载、实例化、方法存在性和API完整性测试
- `test_api_integrity.js` - API完整性和一致性统一测试 (替代了之前的多个重复测试)
- `test_async_operations.js` - 异步操作和Promise支持测试
- `test_complete.js` - 完整的端到端使用示例测试

**功能测试 (需要特定环境)**:
- `test_network.js` - 网络连接测试 (需rigctld)
- `test_serial_config.js` - 串口配置测试 (需硬件)
- `test_new_features.js` - 新功能综合测试 (需硬件)

**测试工具**:
- `run_all_tests.js` - 统一测试运行器，生成详细报告

### Test Execution
```bash
# 运行所有测试 (推荐)
node test/run_all_tests.js

# 运行核心测试 (API验证)
node test/test_loader.js
node test/test_api_integrity.js

# 运行单个功能测试
node test/test_network.js
node test/test_async_operations.js
```

核心测试不需要实际硬件连接，主要验证模块加载、API完整性和接口一致性。功能测试需要相应的硬件或网络环境。

## Important Implementation Details

### Hamlib Token Type Compatibility
项目包含跨平台兼容性处理：
```cpp
#ifndef HAMLIB_TOKEN_T
#ifdef __linux__
#define HAMLIB_TOKEN_T token_t
#else
#define HAMLIB_TOKEN_T hamlib_token_t
#endif
#endif
```

### Memory Management
使用智能指针和RAII模式管理资源，确保RIG对象正确初始化和清理。

### Callback Integration
支持Hamlib回调机制：
- 频率变化回调：`rig_set_freq_callback`
- PTT变化回调：`rig_set_ptt_callback`

## API维护要求 (CRITICAL)

**重要提醒**: 本项目采用三层API架构，任何API变更必须严格遵循以下流程，否则将导致运行时API缺失问题。

### API架构层级
```
TypeScript定义 (index.d.ts) 
    ↓
JavaScript包装器 (lib/index.js) 
    ↓  
C++原生实现 (src/hamlib.cpp)
```

### 🚨 强制性API维护流程

#### 1. 添加新API方法时 (按顺序执行)

**第一步: C++层实现**
- 在 `src/hamlib.h` 中添加方法声明
- 在 `src/hamlib.cpp` 中实现AsyncWorker类和方法
- 在 `src/hamlib.cpp` 的 `DefineClass` 中注册方法
- 确认方法在 `NodeHamLib::InstanceMethod()` 中正确注册

**第二步: JavaScript包装器实现 (必需)**
- **必须** 在 `lib/index.js` 中添加对应的JavaScript包装方法
- 保持与C++方法相同的方法名和参数
- 使用统一的async/Promise模式包装C++调用
- 包装格式示例：
```javascript
async methodName(param1, param2) {
  if (param2) {
    return this._nativeInstance.methodName(param1, param2);
  } else {
    return this._nativeInstance.methodName(param1);
  }
}
```

**第三步: TypeScript定义更新**
- 在 `index.d.ts` 中添加方法的类型定义
- 保持参数类型和返回值类型的准确性
- 添加完整的JSDoc文档和使用示例

**第四步: 验证API一致性**
- 运行 `node test/test_api_integrity.js` 验证API完整性
- 确保所有三个层级的API数量一致
- 验证新方法在运行时可用

#### 2. ⚠️ 常见错误和问题

**JavaScript包装器缺失问题**
- **症状**: C++方法已实现并注册，但运行时显示"方法不存在"
- **原因**: `lib/index.js` 中缺少JavaScript包装方法
- **解决**: 在 `lib/index.js` 中添加对应的包装方法

**API数量不匹配问题**
- **症状**: `test_api_integrity.js` 报告API数量不一致
- **原因**: 三个层级中的某一层缺少方法定义
- **解决**: 按照维护流程逐层检查和补充

**Promise模式不一致**
- **症状**: 某些方法不是async函数或不返回Promise
- **原因**: JavaScript包装器中未使用正确的async/Promise模式
- **解决**: 统一使用 `async methodName() { return this._nativeInstance.methodName(); }`

#### 3. 验证检查清单

在提交任何API变更前，必须确认：
- [ ] C++方法已在 `hamlib.h` 中声明
- [ ] C++方法已在 `hamlib.cpp` 中实现
- [ ] C++方法已在 `DefineClass` 中注册
- [ ] **JavaScript包装方法已在 `lib/index.js` 中实现**
- [ ] TypeScript定义已在 `index.d.ts` 中更新
- [ ] 运行 `node test/test_api_integrity.js` 通过
- [ ] 运行 `node test/test_loader.js` 通过
- [ ] API总数匹配预期 (当前应为55个方法)

#### 4. API维护自动化检查

**API完整性测试**
```bash
# 运行API完整性检查
node test/test_api_integrity.js

# 预期结果：所有测试通过，API数量一致
```

**基础功能测试**
```bash
# 运行基础加载测试
node test/test_loader.js

# 预期结果：所有方法存在性检查通过
```

## 🚨 重要历史问题记录和经验教训

### Split API参数顺序混乱问题 (已修复，但需永久警示)

**⚠️ 严重警告**: 这是一个典型的"注释与实现不一致"导致的系统性问题，曾导致AI反复修改却始终无法对齐的困扰。

#### 问题根源分析

**发现时间**: 2024年8月
**问题性质**: C++实现逻辑错误 + 注释错误的双重问题
**影响范围**: Split相关所有API (setSplit, getSplit, setSplitMode等)

#### 具体问题内容

1. **注释错误** (`src/hamlib.cpp:2957`)
   ```cpp
   // 错误注释: setSplit(enable, txVfo, rxVfo) - txVfo and RX VFO
   // 正确注释: setSplit(enable, rxVfo, txVfo) - RX VFO and TX VFO
   ```

2. **实现逻辑错误** (`src/hamlib.cpp:2969-2970`)
   ```cpp
   // 错误实现:
   std::string txVfoStr = info[1].As<Napi::String>().Utf8Value();  // 将参数1赋给txVfo!
   std::string rxVfoStr = info[2].As<Napi::String>().Utf8Value();  // 将参数2赋给rxVfo!
   
   // 正确实现:
   std::string rxVfoStr = info[1].As<Napi::String>().Utf8Value();  // 参数1应该是rxVfo
   std::string txVfoStr = info[2].As<Napi::String>().Utf8Value();  // 参数2应该是txVfo
   ```

3. **三层API定义不一致**
   - JavaScript: `setSplit(enable, rxVfo, txVfo)` ✅ 正确
   - C++实现: 按 `setSplit(enable, txVfo, rxVfo)` 处理 ❌ 错误
   - TypeScript: `setSplit(enable, rxVfo?, txVfo?)` ✅ 正确

#### 为什么AI反复修改失败

**根本原因**: AI每次只看到表面的注释问题，没有深入检查实现逻辑
- 第1次修改: 只改注释，没有检查实现
- 第2次修改: 发现注释还是不对，又改注释，还是没检查实现
- 第N次修改: 陷入"注释修改循环"，始终没有发现真正的实现逻辑错误

#### 修正措施 (已完成)

1. **修正注释**: 统一所有注释为 `setSplit(enable, rxVfo, txVfo)`
2. **修正实现**: 确保参数1是rxVfo，参数2是txVfo
3. **修正变量赋值**: 调整参数解析的变量赋值顺序
4. **添加专门测试**: `test_split_api_fix.js` 验证修正的正确性

#### 关键经验教训

**🔥 对AI的重要提醒**:

1. **不要只看注释，必须检查实现逻辑**
   - 注释可能是错误的或过时的
   - 实际的参数处理代码才是真相

2. **参数顺序问题的完整检查流程**
   ```
   步骤1: 检查JavaScript/TypeScript定义的参数顺序
   步骤2: 检查C++方法签名和注释
   步骤3: 检查C++实现中参数的实际处理逻辑 (关键!)
   步骤4: 检查原生Hamlib API的参数顺序
   步骤5: 确保所有层级完全一致
   ```

3. **三层API架构一致性验证**
   - JavaScript定义 ↔ C++实现 ↔ TypeScript定义
   - 任何一层不一致都会导致运行时错误

4. **避免"修改循环陷阱"**
   - 如果多次修改同一个API还是有问题，说明可能存在更深层的逻辑错误
   - 必须从实现层面重新检查，而不是继续在表面修改

#### 验证方法

**每当修改涉及参数顺序的API时，必须**:
1. 运行专门的参数测试: `node test/test_split_api_fix.js`
2. 确认所有相关测试通过: `node test/test_api_integrity.js`
3. 手动验证参数处理逻辑与注释一致

**警示标识**: 在 `src/hamlib.cpp` 的Split相关代码附近已添加特殊注释标记，提醒未来的维护者注意这个历史问题。

## Development Notes

当修改API时，除了遵循上述API维护流程外，还需确保：
1. 保持Promise模式一致性
2. 处理跨平台兼容性  
3. 更新相关测试文件
4. 验证异步操作的正确性
5. **特别注意参数顺序问题** (参考上述Split API的历史教训)

构建问题排查：
1. 检查Hamlib库是否正确安装在系统中
2. 验证node-gyp和Python环境配置
3. 查看binding.gyp中的路径配置是否适合当前系统