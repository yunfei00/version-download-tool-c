# Version Download Tool (C Edition)

一个纯 **C + Win32 API** 的轻量级 Windows 版本下载工具示例，不依赖 Qt/.NET/Python，目标产物为单个 `exe`。

## 功能（当前版本）

- 输入 GitHub Repo（`owner/repo`）后，点击 `Refresh Releases` 实时请求：
  - `https://api.github.com/repos/{owner}/{repo}/releases`
- 使用 WinHTTP HTTPS GET，请求头包含：
  - `User-Agent: version-download-tool-c`
  - `Accept: application/vnd.github+json`
- 轻量级 JSON 解析并展示 Release 资产（asset）到表格：
  - Version = `tag_name`
  - File Name = `assets.name`
  - Size = 自动格式化为 KB/MB
  - Published At = `published_at`
  - Status = `Ready`
- 支持最多 256 个资产项。
- `Download Selected` 下载选中资产到当前目录 `downloads/`。
- 下载和刷新均使用 worker thread，避免 UI 卡死。
- 下载进度实时更新到底部进度条，并在日志输出开始、路径、完成/失败。

## 本地构建（MinGW-w64）

```bash
gcc src/main.c src/app.c src/downloader.c resources/app.rc -o version-download-tool.exe -mwindows -lcomctl32 -lwinhttp
```

或在 Windows CMD 中执行：

```bat
build.bat
```

## GitHub Actions 打包

工作流文件：`.github/workflows/build-windows.yml`

- 运行环境：`windows-2022`
- 使用 MinGW-w64 编译可执行文件
- 上传 `version-download-tool.exe` 为 artifact
- 当触发 tag（`v*`）时，自动创建 GitHub Release 并附带 exe

## 代码结构

- `src/app.c`：GUI、布局、消息循环、线程启动、表格和日志更新
- `src/downloader.c`：GitHub Releases 拉取、JSON 字段提取、文件下载
- `src/downloader.h`：`ReleaseAsset` 结构和下载/拉取接口

## 错误处理

- Repo 格式错误：`Invalid repo format. Expected owner/repo.`
- 无可用资产：`No release assets found.`
- 下载未选中行：`Please select a release asset first.`
- HTTP/Win32 错误会输出到日志框
