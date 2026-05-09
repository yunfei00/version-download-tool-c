# Version Download Tool (C Edition)

一个纯 **C + Win32 API** 的轻量级 Windows 版本下载工具示例。

## 功能

- 输入 `owner/repo` 后，点击 `Refresh Releases` 拉取 GitHub Release assets。
- 顶部支持 `Select Folder` 选择下载目录（默认 `当前目录/downloads`）。
- `Download Selected` 下载选中资产，支持进度、日志、状态同步。
- `Cancel Download` 可中断下载，状态更新为 `Cancelled`。
- 文件先写入 `.part`，成功后再重命名；同名文件自动追加 `_1/_2` 避免覆盖。
- ListView 支持整行选中、网格线、单选，并使用固定列宽。

## 使用说明

1. 输入 repo：`owner/repo`
2. 点击 `Refresh Releases`
3. 选择 asset
4. 选择下载目录
5. 点击 `Download Selected`

## 构建

```bat
build.bat
```

## GitHub Actions / Release

- 工作流：`.github/workflows/build-windows.yml`
- 使用 `windows-2022` 构建并上传 artifact。
- tag `v*` 会创建 GitHub Release 并上传 `version-download-tool.exe`。

打 tag 发布：

```bash
git tag v0.1.0
git push origin v0.1.0
```

产物会出现在 GitHub Actions **Artifacts** 和 **Releases** 中。

## 截图

- 功能截图占位：`docs/screenshot-placeholder.png`（后续补充真实截图）
