# Node-HamLib 发布指南

## 发布流程

```bash
# 1. 确保代码在 main 分支且 CI 通过
# 2. bump 版本（自动创建 commit + v* tag）
npm version patch   # bug 修复
npm version minor   # 新功能
npm version major   # 破坏性更改

# 3. 推送代码和 tag
git push && git push --tags
```

推送 tag 后 CI 自动完成：
1. 5 平台构建 + 测试（linux-x64, linux-arm64, darwin-arm64, darwin-x64, win32-x64）
2. 汇总 prebuilds
3. 验证全部平台二进制完整
4. `npm publish` 发布到 npm registry
5. 创建 GitHub Release 并上传各平台预构建压缩包

## 前置条件

在 GitHub repo settings > Secrets and variables > Actions 中添加：
- `NPM_TOKEN`：npm access token（`npm token create` 生成）

## 预构建包结构

```
prebuilds/
├── linux-x64/
│   ├── node.napi.node
│   ├── libhamlib.so.5
│   └── ...
├── linux-arm64/
│   └── ...
├── darwin-arm64/
│   ├── node.napi.node
│   ├── libhamlib.4.dylib
│   └── ...
├── darwin-x64/
│   └── ...
└── win32-x64/
    ├── node.napi.node
    ├── hamlib_shim.dll
    ├── libhamlib-4.dll
    └── ...
```

运行时通过 `node-gyp-build` 自动加载对应平台的预构建二进制。

## 用户安装

```bash
npm install hamlib
# 预构建二进制随包安装，无需编译
```

无对应预构建的平台需从源码构建（需要 Hamlib 开发库、node-gyp、C++ 编译器）。
