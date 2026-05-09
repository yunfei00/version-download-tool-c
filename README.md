# Version Download Tool (C Edition)

一个纯 **C + Win32 API** 的轻量级 Windows 版本下载工具示例。

## 功能

- 使用可输入的 Repo 下拉框（ComboBox），支持最近 10 个仓库历史。
- 程序启动自动读取 `app_config.ini`，退出自动保存配置。
- 记住上次 Repo、下载目录、窗口大小。
- `Refresh Releases` 拉取 GitHub Release assets。
- `Check Self Update` 检查 `yunfei00/version-download-tool-c` 最新版本。
- 顶部支持 `Select Folder` 选择下载目录。
- `Download Selected` 下载选中资产，支持进度、日志、状态同步。

## 配置文件

配置文件路径：程序当前目录 `app_config.ini`。

示例：

```ini
repo=yunfei00/version-download-tool-c
download_dir=downloads
window_width=900
window_height=560
recent_repo_1=yunfei00/version-download-tool-c
recent_repo_2=owner/repo
```

- 文件不存在时使用默认值，并在退出时自动创建。
- `recent_repo_n` 最多保存 10 条。

## 构建

```bat
build.bat
```

本地构建默认 `BUILD_VERSION="local"`。

## GitHub Actions / Release

- 工作流：`.github/workflows/build-windows.yml`
- 使用 `windows-2022` 构建并上传 artifact。
- Tag 构建注入 `BUILD_VERSION=tag 名`，普通 push 注入短 SHA。
- tag `v*` 会用 `softprops/action-gh-release@v3` 创建 Release 并上传 `version-download-tool.exe`。

打 tag 发布：

```bash
git tag v0.1.0
git push origin v0.1.0
```
