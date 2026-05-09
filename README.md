# Version Download Tool (C Edition)

一个纯 **C + Win32 API** 的轻量级 Windows 版本下载工具示例，不依赖 Qt/.NET/Python，目标产物为单个 `exe`。

## 功能（第一版）

- 主窗口标题：`Version Download Tool - C Edition`
- 默认窗口尺寸：`900x560`
- 顶部：
  - GitHub Repo 输入框（默认 `yunfei00/version-download-tool-c`）
  - `Refresh Releases` 按钮
  - `Download Selected` 按钮
- 中部：ListView 版本表格（列：No、Version、File Name、Size、Published At、Status）
- 底部：下载进度条 + 日志输出框
- 当前使用 mock 数据填充版本列表（暂不解析 GitHub API）
- 点击 `Refresh Releases` 会重载 mock 数据
- 点击 `Download Selected` 后会异步模拟下载进度（0%~100%），不中断 UI

## 项目结构

```text
version-download-tool-c/
├─ README.md
├─ .gitignore
├─ build.bat
├─ src/
│  ├─ main.c
│  ├─ app.h
│  ├─ app.c
│  ├─ downloader.h
│  └─ downloader.c
├─ resources/
│  └─ app.rc
└─ .github/
   └─ workflows/
      └─ build-windows.yml
```

## 本地构建（MinGW-w64）

```bash
gcc src/main.c src/app.c src/downloader.c resources/app.rc -o version-download-tool.exe -mwindows -lcomctl32 -lwinhttp
```

或在 Windows CMD 中执行：

```bat
build.bat
```

## GitHub Actions

工作流文件：`.github/workflows/build-windows.yml`

- 运行环境：`windows-2022`
- 触发条件：
  - `push` 到 `main`
  - `pull_request`
  - `workflow_dispatch`
  - 推送 tag（如 `v0.1.0`）
- 输出：上传 `version-download-tool.exe` 到 artifacts
- 当触发 tag（`v*`）时，自动创建 GitHub Release 并附带 exe

## 代码设计说明

- `src/app.c`：GUI 创建、布局、事件分发、日志、列表操作
- `src/downloader.c`：模拟下载线程逻辑（worker thread）
- `app_log`：统一日志输出接口
- `app_add_release_row`：统一 ListView 行插入接口

## 后续可扩展方向

- 接入 GitHub Releases API（WinHTTP）
- 真实文件下载、断点续传、校验摘要
- 配置持久化与历史记录
