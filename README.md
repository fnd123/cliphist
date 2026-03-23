# cliphist

`cliphist` 是一个本地优先的剪贴板历史工具，支持历史记录、搜索过滤、收藏、导出，以及桌面托盘 + 图形界面。

目前项目支持：

- Linux（X11）
- Windows（Qt5）

## 功能概览

- 持续监听剪贴板并写入本地 SQLite 数据库
- 基于内容去重，重复内容会更新命中次数和时间
- 支持按关键词、精确内容、时间范围查询
- 支持命令行查看、统计、导出
- 支持桌面托盘和图形界面
- 支持收藏、删除、清空、编辑草稿、导出文本 / JSON

## 安装

### 从 Release 安装

打开 GitHub Releases，下载对应平台的安装包。

Linux 推荐使用 `.deb`：

```bash
sudo apt install ./cliphist-<version>-Linux.deb
```

如果你使用的是 `dpkg -i`，安装后还需要执行：

```bash
sudo apt -f install
```

Windows 提供两种包：

- `cliphist-*.exe`
  适合普通安装，带开始菜单和卸载入口
- `cliphist-*.zip`
  适合便携使用，解压后直接运行

### 从源码构建

Linux：

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

Windows（MSYS2 / MinGW64）：

```bash
cmake -S . -B build-win -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCLIPHIST_ENABLE_X11=OFF ^
  -DCMAKE_PREFIX_PATH=/mingw64
cmake --build build-win
ctest --test-dir build-win --output-on-failure
```

Windows 构建前建议先安装这些包：

```bash
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-qt5-base \
  mingw-w64-x86_64-sqlite3 \
  mingw-w64-x86_64-nsis
```

## 快速开始

### Linux

启动后台监听：

```bash
cliphist daemon --selection clipboard
```

启动托盘 + 后台监听 + 图形界面：

```bash
cliphist desktop
```

### Windows

Windows 下直接双击或无参数启动，默认进入 `desktop` 模式。

也可以显式启动：

```bash
cliphist desktop
```

查看图形界面：

```bash
cliphist ui --limit 200
```

## 常用命令

查看最近 50 条历史：

```bash
cliphist list --limit 50
```

搜索包含 `hello` 的内容：

```bash
cliphist list --contains hello
```

精确匹配：

```bash
cliphist list --exact "hello world"
```

查看某个时间戳之后的内容：

```bash
cliphist list --since 1700000000
```

按 JSON 输出：

```bash
cliphist list --limit 50 --json
```

只统计匹配数量：

```bash
cliphist list --contains hello --count-only
```

查看统计信息：

```bash
cliphist stats
cliphist stats --json
```

## 图形界面怎么用

图形界面主要分成左侧历史列表和右侧预览/编辑区。

常用操作：

- 单击历史项：查看内容
- 双击历史项：复制当前草稿/内容
- 搜索框：按关键词实时过滤
- `加入收藏`：把当前记录放到收藏组
- `删除选中`：删除当前记录
- `自动复制`：切换“选中即复制”
- `更多` 菜单：
  - 暂停监听
  - 清空历史
  - 窗口置顶
  - 导出文本
  - 导出 JSON

说明：

- `desktop` 模式会同时启动监听、托盘和图形界面
- `ui` 只打开窗口，不负责长期后台托管
- Windows 托盘点击可唤起或聚焦窗口

## 数据保存位置

默认数据库位置：

- Linux：`~/.local/share/cliphist/cliphist.db`
- Windows：`%LOCALAPPDATA%/cliphist/cliphist.db`

也可以通过 `--db PATH` 指定自定义数据库。

同目录下还会生成这些内容：

- `archive/`：按日期归档的文本副本
- `exports/`：导出的文本或 JSON
- `pause.state`：暂停监听状态
- `logs/cliphist.log`：Windows 启动和运行日志

## 命令说明

### `cliphist daemon`

后台监听剪贴板。

常用参数：

- `--max-items N`
  最大保留条数，`0` 表示不限
- `--db PATH`
  指定数据库路径
- `--selection clipboard|primary|both`
  仅 Linux X11 可用

### `cliphist desktop`

启动完整桌面模式。

- Linux：托盘 + 监听 + UI
- Windows：Qt 托盘 + 监听 + UI

### `cliphist ui`

只打开图形界面。

常用参数：

- `--limit N`
  UI 初始显示条数，上限为 `10000`
- `--db PATH`
  指定数据库路径

### `cliphist list`

按条件列出历史记录。

支持参数：

- `--limit N`
- `--offset N`
- `--contains TEXT`
- `--exact TEXT`
- `--since UNIX_TS`
- `--sort updated_at|created_at`
- `--order asc|desc`
- `--count-only`
- `--json`
- `--db PATH`

### `cliphist stats`

输出数据库统计信息。

支持参数：

- `--json`
- `--db PATH`

## 打包

Linux：

```bash
cpack --config build/CPackConfig.cmake
```

Windows：

```bash
cpack --config build-win/CPackConfig.cmake -C Release
```

## 发布

推送 `main` 会触发 nightly 流水线。

推送标签会触发正式 Release：

```bash
git tag v0.2.0
git push origin v0.2.0
```

## 项目结构

- `src/`：核心代码
- `tests/`：单元测试和集成测试
- `sql/`：数据库 schema
- `docs/`：架构、CLI、打包、发布说明

## 相关文档

- [架构说明](docs/architecture.md)
- [命令行说明](docs/cli.md)
- [Windows 打包](docs/windows-packaging.md)
- [GitHub 自动发布](docs/github-release.md)
