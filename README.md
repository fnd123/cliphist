# cliphist

`cliphist` 是一个本地优先的剪贴板历史管理工具，支持去重、统计、查询、收藏、导出和桌面 UI。

## 功能特性

- 剪贴板历史记录与去重（基于内容哈希）
- 历史查询过滤（`contains` / `exact` / `since`）
- 统计信息（总条数、总命中、时间范围）
- 收藏、删除、清空、导出（文本 / JSON）
- Qt 图形界面（查看、搜索、编辑、复制草稿）
- 可打包为 Linux 安装包和 Windows 安装包

## 平台支持

| 平台 | 支持命令 |
| --- | --- |
| Linux（X11 构建） | `daemon` / `desktop` / `ui` / `list` / `stats` |
| Windows（非 X11 构建） | `ui` / `list` / `stats` |

说明：`daemon`、`desktop` 依赖 Linux X11，Windows 构建中会禁用。

## 安装

### 从 Release 安装

1. 打开 GitHub Releases，下载对应平台安装包。
2. Linux 推荐安装 `.deb`：

```bash
sudo apt install ./cliphist-<version>-Linux.deb
```

`apt install ./xxx.deb` 会自动解析并安装依赖。  
如果使用 `dpkg -i`，需要再执行 `sudo apt -f install` 修复依赖。

3. Windows 可使用：
- `cliphist-*.exe`（NSIS 安装器）
- `cliphist-*.zip`（便携包）

## 快速开始

```bash
# Linux: 启动监听
cliphist daemon --max-items 1000 --selection clipboard

# Linux: 默认无限保留（不传 --max-items 或传 0）
cliphist daemon --selection clipboard

# Linux: 一体化桌面会话（托盘 + UI + daemon）
cliphist desktop

# 通用: 查看历史
cliphist list --limit 50

# 通用: 条件过滤
cliphist list --contains hello --since 1700000000

# 通用: 统计
cliphist stats --json

# 通用: 打开 UI
cliphist ui --limit 200
```

说明：
- `ui` / `desktop` 默认历史显示上限为 `10000` 条（超过会自动截断）。
- UI 分组为 `今天 YYYY/MM/DD`、`昨天 YYYY/MM/DD`、`更久以前`、`收藏`，默认仅展开 `今天`。

## 从源码构建

### Linux（默认启用 X11）

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Windows（禁用 X11）

```bash
cmake -S . -B build-win -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCLIPHIST_ENABLE_X11=OFF
cmake --build build-win
ctest --test-dir build-win --output-on-failure
```

### 打包

```bash
# Linux
cpack --config build/CPackConfig.cmake

# Windows
cpack --config build-win/CPackConfig.cmake -C Release
```

## GitHub Actions 自动发布

仓库已配置工作流：`.github/workflows/build-and-release.yml`

- `pull_request`：Linux + Windows 构建测试
- `push main`：构建测试并上传 artifacts
- `push tag v*`：自动创建 Release 并上传安装包

发布示例：

```bash
git tag v0.1.1
git push origin v0.1.1
```

## 项目结构

- `src/`：核心源码（app / cli / core / db / x11 / ui / util）
- `tests/`：单元测试与集成测试
- `sql/`：数据库 schema
- `docs/`：架构、CLI、Windows 打包、GitHub 发布文档

## 参考文档

- [架构说明](docs/architecture.md)
- [命令行说明](docs/cli.md)
- [Windows 打包](docs/windows-packaging.md)
- [GitHub 自动发布](docs/github-release.md)
