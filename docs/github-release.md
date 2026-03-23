# GitHub 自动编译与发布

仓库已配置工作流：
- `.github/workflows/build-and-release.yml`

## 触发规则

- `pull_request`：构建与测试（Linux + Windows），并上传安装包 artifacts。
- 推送到 `main`：构建、测试、打包并自动创建 **nightly 预发布**（`prerelease`）。
- 推送标签 `v*`（如 `v0.2.1`）：构建、测试、打包并自动创建 **正式 Release**。

## 发布步骤

1. 确保 `main` 是要发布的代码。
2. 自动 nightly（无需打标签）

只要代码 push 到 `main`，工作流会自动创建一个唯一 nightly tag（例如 `nightly-20260309-1005-abc1234`）并发布预发布版本。

3. 正式发布（推荐）

本地打标签并推送：

```bash
git tag v0.2.1
git push origin v0.2.1
```

4. 等待 Actions 工作流完成。
5. 在 GitHub 的 `Releases` 页面查看自动创建的版本和附件。

## 产物说明

- Linux: `.deb` + `.tar.gz`
- Windows: `.exe` (NSIS) + `.zip`

## 常见问题

- Windows 打包失败（找不到 NSIS）：检查工作流中 `nsis` 安装步骤是否成功。
- Linux 缺少依赖：检查 `apt-get install` 列表。
- 标签推送后未发正式版：确认标签名以 `v` 开头（例如 `v1.2.3`）。
