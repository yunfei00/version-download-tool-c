# Lightweight HTTP Path Download Tool

纯 **C + Win32 API** 的轻量级 Windows 下载工具（单 exe）。

## 功能

- 单文件 URL 下载（HTTP/HTTPS）
- 多 URL 批量下载（多行输入 + Add URL）
- HTTP 目录索引扫描（nginx/apache 常见目录页）
- Download Selected / Download All
- 下载进度、总体进度、速度、ETA 日志
- 使用 `.part` 临时文件，完成后重命名
- 文件已存在时自动重命名，避免覆盖

## 构建

```bat
build.bat
```

保持 MSYS2 UCRT64 构建方式，产物为单一 exe。
