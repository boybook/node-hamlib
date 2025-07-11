#!/bin/bash
# release.sh - Node-Hamlib 半自动化发布脚本
# 使用方法: ./release.sh [patch|minor|major]

set -e  # 遇到错误立即退出

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 输出函数
log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_success() {
    echo -e "${GREEN}✅ $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

log_error() {
    echo -e "${RED}❌ $1${NC}"
}

# 检查参数
VERSION_TYPE=${1:-patch}
if [[ ! "$VERSION_TYPE" =~ ^(patch|minor|major)$ ]]; then
    log_error "无效的版本类型: $VERSION_TYPE"
    echo "使用方法: $0 [patch|minor|major]"
    echo "  patch: 修订版本 (0.1.0 -> 0.1.1)"
    echo "  minor: 次版本 (0.1.0 -> 0.2.0)"
    echo "  major: 主版本 (0.1.0 -> 1.0.0)"
    exit 1
fi

echo "🚀 开始node-hamlib发布流程..."
echo "版本更新类型: $VERSION_TYPE"

# 检查当前目录
if [ ! -f "package.json" ]; then
    log_error "请在项目根目录运行此脚本"
    exit 1
fi

# 检查是否已登录npm
log_info "检查npm登录状态..."
if ! npm whoami > /dev/null 2>&1; then
    log_error "请先登录npm: npm login"
    exit 1
fi
log_success "npm已登录: $(npm whoami)"

# 检查git状态
log_info "检查git工作区状态..."
if [ -n "$(git status --porcelain)" ]; then
    log_warning "工作区有未提交的更改"
    git status --short
    read -p "继续发布？(y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_error "发布已取消"
        exit 1
    fi
fi

# 检查当前分支
CURRENT_BRANCH=$(git branch --show-current)
if [ "$CURRENT_BRANCH" != "main" ] && [ "$CURRENT_BRANCH" != "master" ]; then
    log_warning "当前不在main/master分支: $CURRENT_BRANCH"
    read -p "继续发布？(y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_error "发布已取消"
        exit 1
    fi
fi

# 检查prebuilds目录
log_info "检查预构建文件..."
if [ ! -d "prebuilds" ]; then
    log_error "prebuilds目录不存在，请先按照MANUAL_RELEASE.md下载预构建文件"
    exit 1
fi

if [ -z "$(ls -A prebuilds)" ]; then
    log_error "prebuilds目录为空，请先按照MANUAL_RELEASE.md下载预构建文件"
    exit 1
fi

# 验证预构建文件
log_info "验证预构建文件完整性..."
PLATFORMS=("linux-x64" "linux-arm64" "darwin-arm64")
MISSING_PLATFORMS=()

for platform in "${PLATFORMS[@]}"; do
    if [ -f "prebuilds/$platform/hamlib.node" ]; then
        # 检查文件大小（应该大于10KB）
        file_size=$(stat -f%z "prebuilds/$platform/hamlib.node" 2>/dev/null || stat -c%s "prebuilds/$platform/hamlib.node" 2>/dev/null)
        if [ "$file_size" -gt 10000 ]; then
            log_success "$platform: OK (${file_size} bytes)"
        else
            log_warning "$platform: 文件过小 (${file_size} bytes)"
            MISSING_PLATFORMS+=("$platform")
        fi
    else
        log_error "$platform: 缺失"
        MISSING_PLATFORMS+=("$platform")
    fi
done

if [ ${#MISSING_PLATFORMS[@]} -gt 0 ]; then
    log_error "以下平台的预构建文件缺失或异常: ${MISSING_PLATFORMS[*]}"
    log_info "请检查GitHub Actions构建状态并重新下载artifacts"
    exit 1
fi

# 安装依赖
log_info "安装依赖..."
npm ci

# 运行测试
log_info "运行测试..."
if ! npm test; then
    log_error "测试失败，发布已取消"
    exit 1
fi
log_success "所有测试通过"

# 测试预构建文件加载
log_info "测试预构建文件加载..."
cat > test-prebuilt.js << 'EOF'
const hamlib = require('./index.js');
console.log('✓ Successfully loaded node-hamlib');
if (typeof hamlib === 'object' && hamlib !== null) {
    console.log('Available functions:', Object.keys(hamlib));
    console.log('✓ Module export structure looks correct');
} else {
    console.error('✗ Module export structure is invalid');
    process.exit(1);
}
EOF

if ! node test-prebuilt.js; then
    log_error "预构建文件加载测试失败"
    rm -f test-prebuilt.js
    exit 1
fi
rm -f test-prebuilt.js
log_success "预构建文件加载测试通过"

# 检查包结构
log_info "检查包结构..."
npm pack --dry-run > pack-output.txt 2>&1

log_info "将要打包的文件："
grep -E "(prebuilds|index\.|lib/|package\.json|COPYING|Readme\.md)" pack-output.txt || true

# 检查prebuilds是否包含在包中 - 修复：检查实际的文件路径
if ! grep -q "prebuilds.*hamlib\.node" pack-output.txt; then
    log_error "prebuilds目录未包含在包中，请检查package.json中的files字段"
    rm -f pack-output.txt
    exit 1
fi

# 检查所有平台的二进制文件是否都包含
for platform in "${PLATFORMS[@]}"; do
    if ! grep -q "prebuilds/$platform/hamlib\.node" pack-output.txt; then
        log_error "$platform/hamlib.node 未包含在包中"
        rm -f pack-output.txt
        exit 1
    fi
done

rm -f pack-output.txt
log_success "包结构检查通过"

# 获取当前版本
CURRENT_VERSION=$(node -p "require('./package.json').version")
log_info "当前版本: $CURRENT_VERSION"

# 预览新版本 - 修复：使用semver计算而不是npm version --dry-run
case $VERSION_TYPE in
    "patch")
        PREVIEW_VERSION="v$(echo $CURRENT_VERSION | awk -F. '{print $1"."$2"."($3+1)}')"
        ;;
    "minor")
        PREVIEW_VERSION="v$(echo $CURRENT_VERSION | awk -F. '{print $1"."($2+1)".0"}')"
        ;;
    "major")
        PREVIEW_VERSION="v$(echo $CURRENT_VERSION | awk -F. '{print ($1+1)".0.0"}')"
        ;;
esac
log_info "新版本将是: $PREVIEW_VERSION"

# 确认发布
echo
echo "📋 发布摘要:"
echo "  • 包名: $(node -p "require('./package.json').name")"
echo "  • 当前版本: $CURRENT_VERSION"
echo "  • 新版本: $PREVIEW_VERSION"
echo "  • 平台支持: ${PLATFORMS[*]}"
echo "  • npm用户: $(npm whoami)"
echo

read -p "确认发布到npm？(y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    log_error "发布已取消"
    exit 1
fi

# 更新版本号
log_info "更新版本号到 $PREVIEW_VERSION..."
npm version $VERSION_TYPE --no-git-tag-version

# 创建测试包
log_info "创建测试包..."
PACKAGE_FILE=$(npm pack 2>/dev/null | tail -1)
log_success "创建包文件: $PACKAGE_FILE"

# 最终检查包内容
log_info "验证包内容..."
tar -tzf "$PACKAGE_FILE" | grep -E "prebuilds.*\.node$" | while read file; do
    log_success "包含: $file"
done

# 发布到npm
log_info "发布到npm..."
if npm publish "$PACKAGE_FILE"; then
    log_success "成功发布到npm!"
    
    # 清理包文件
    rm -f "$PACKAGE_FILE"
    
    # 提交版本更改
    log_info "提交版本更改..."
    git add package.json package-lock.json 2>/dev/null || git add package.json
    git commit -m "chore: bump version to $PREVIEW_VERSION"
    
    # 创建git标签
    git tag "v$PREVIEW_VERSION"
    
    # 推送更改和标签
    log_info "推送更改和标签到远程仓库..."
    git push origin "$CURRENT_BRANCH"
    git push origin "v$PREVIEW_VERSION"
    
    echo
    log_success "🎉 发布完成!"
    echo "  • npm包: https://www.npmjs.com/package/hamlib"
    echo "  • 版本: $PREVIEW_VERSION"
    echo "  • 标签: v$PREVIEW_VERSION"
    echo
    log_info "可以通过以下命令安装:"
    echo "  npm install hamlib@$PREVIEW_VERSION"
    
else
    log_error "npm发布失败"
    rm -f "$PACKAGE_FILE"
    
    # 回滚版本号
    log_info "回滚版本号..."
    git checkout -- package.json package-lock.json 2>/dev/null || git checkout -- package.json
    
    exit 1
fi 