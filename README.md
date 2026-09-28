# Local API

C++17 本地 HTTP 服务，基于请求体 action 字段分发工具调用，集成 Root 检测（KernelSU / APatch / Magisk / SusFS）。

## 架构

```
main.cpp      → HTTP 网卡（只启动服务、绑定路由）
router.cpp    → 请求分发（解析 action，路由到对应工具函数）
tools.cpp     → 工具函数集合（detect / version / debug / 图标与隐藏配置 / 占位工具）
detector.cpp  → 纯检测库（KSU / APatch / Magisk / SusFS 握手）
version.hpp   → 版本号 + 配置常量
```

客户端只需 `POST /`，请求体 `{"action": "..."}` 决定调用哪个工具。

## 项目结构

```
./
├── Makefile                     # 主机 + NDK 构建脚本
├── src/
│   ├── main.cpp                 # 仅 HTTP 网卡（服务绑定）
│   ├── router.cpp               # 请求分发（action → tool）
│   ├── tools.cpp                # 工具函数集合
│   └── detector.cpp             # 纯检测库
├── include/
│   ├── version.hpp              # 版本号 + 配置
│   ├── router.hpp               # 路由分发接口
│   ├── tools.hpp                # 工具函数注册表
│   ├── detector.hpp             # 检测库接口
│   ├── ksu_uapi.hpp             # KernelSU 用户态 API
│   ├── apatch_uapi.hpp          # APatch supercall
│   ├── magisk_uapi.hpp          # Magisk daemon 协议
│   └── susfs_uapi.hpp           # SusFS 握手 ABI
├── third_party/
│   ├── httplib.h                # cpp-httplib
│   └── nlohmann/json.hpp        # nlohmann/json
├── jni/                         # NDK 构建配置
├── test_api.sh                  # 测试脚本
└── README.md
```

## API

### POST / — 所有请求入口

请求体 JSON，`action` 字段决定调用哪个工具。

> ⚠️ **权限说明**：`hide_icon`、`susfs_setup`、`hide_app_list`、`update_key`、`set_hash` 五个工具需要以 **root 权限**运行服务，否则返回 `{"status":"error","message":"root privileges required"}`。

#### action: detect — Root 检测

```bash
curl -X POST http://localhost:8080/ -H "Content-Type: application/json" -d '{"action":"detect"}'
```

```json
{
  "status": "ok",
  "result_file": "/data/local/tmp/coverRoot/root_detect.json",
  "result": {
    "detected": "kernelsu",
    "kernelsu": {"present": true, "mode": "lkm-bundled"},
    "apatch": {"present": false},
    "magisk": {"present": false},
    "susfs": {"present": true}
  }
}
```

#### action: version — 版本信息

```bash
curl -X POST http://localhost:8080/ -d '{"action":"version"}'
```

```json
{"status":"ok","server_version":"1.1.0","api_version":"2.0","app_name":"Local-api"}
```

#### action: debug — 调试信息 + 工具试运行

```bash
curl -X POST http://localhost:8080/ -d '{"action":"debug"}'
```

返回所有工具列表、版本、状态、是否 root、以及检测试运行结果。

#### action: hide_icon — 隐藏 / 恢复应用图标（需要 root）

```bash
# 隐藏图标
curl -X POST http://localhost:8080/ -d '{"action":"hide_icon","enabled":true}'
# 恢复图标
curl -X POST http://localhost:8080/ -d '{"action":"hide_icon","enabled":false}'
```

底层执行 `pm disable` / `pm enable com.coverRoot/.LauncherAlias`。

```json
{"status":"ok","message":"icon hidden","enabled":true}
```

#### action: susfs_setup — 一键配置 SusFS 隐藏路径（需要 root）

```bash
curl -X POST http://localhost:8080/ -d '{
  "action":"susfs_setup",
  "paths":["/data/adb/ksu","/system/xbin/su"]
}'
```

路径必须为绝对路径，且不能包含 `..` 或 shell 特殊字符。配置持久化到 `/data/local/tmp/coverRoot/susfs_paths.json`。

```json
{
  "status":"ok",
  "message":"SusFS paths configured",
  "configured_paths":["/data/adb/ksu","/system/xbin/su"]
}
```

#### action: hide_app_list — 一键配置隐藏应用列表（需要 root）

```bash
curl -X POST http://localhost:8080/ -d '{
  "action":"hide_app_list",
  "packages":["com.example.app1","com.example.app2"]
}'
```

包名仅允许字母、数字、`.`、`_`，配置持久化到 `/data/local/tmp/coverRoot/hidden_apps.json`。

```json
{
  "status":"ok",
  "message":"hidden app list configured",
  "hidden_packages":["com.example.app1","com.example.app2"]
}
```

#### action: update_key — 更新认证密钥（需要 root）

```bash
curl -X POST http://localhost:8080/ -d '{
  "action":"update_key",
  "key":"new_secret_key_value"
}'
```

密钥长度 1–256，不允许 shell 特殊字符；持久化到 `/data/local/tmp/coverRoot/auth_key`。

```json
{"status":"ok","message":"Key updated"}
```

#### action: set_hash — 设置模块 / 文件哈希（需要 root）

```bash
curl -X POST http://localhost:8080/ -d '{
  "action":"set_hash",
  "hash":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
}'
```

哈希为 32–128 位十六进制字符（如 sha256），持久化到 `/data/local/tmp/coverRoot/module.sha256`。

```json
{
  "status":"ok",
  "message":"hash updated",
  "hash":"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
}
```

### GET /debug — 调试端点

等同 `action: debug`，方便浏览器直接访问：

```bash
curl http://localhost:8080/debug
```

### 已注册工具

| action | 状态 | 说明 |
|--------|------|------|
| `detect` | ✅ 已实现 | Root 检测（KSU/APatch/Magisk/SusFS 握手） |
| `version` | ✅ 已实现 | 服务器版本 + API 版本 |
| `debug` | ✅ 已实现 | 工具信息 + 检测试运行 + 环境信息 |
| `sysinfo` | 🔲 占位 | 系统信息（设备型号、Android 版本、内核等） |
| `modules` | 🔲 占位 | 模块管理（列表、启用/禁用） |
| `config` | 🔲 占位 | 配置读写 |
| `hide_icon` | ✅ 已实现 | 隐藏 / 恢复应用图标（需要 root） |
| `susfs_setup` | ✅ 已实现 | 一键配置 SusFS 隐藏路径（需要 root） |
| `hide_app_list` | ✅ 已实现 | 一键配置隐藏应用列表（需要 root） |
| `update_key` | ✅ 已实现 | 更新认证密钥（需要 root） |
| `set_hash` | ✅ 已实现 | 设置模块 / 文件哈希（需要 root） |

### 未知 action

```json
{"status":"error","error":"unknown action: xxx","available_actions":["detect","version","debug","sysinfo","modules","config","hide_icon","susfs_setup","hide_app_list","update_key","set_hash"]}
```

## 构建

```bash
# 主机编译
make

# Android NDK
make ndk NDK_HOME=/path/to/ndk

# 清理
make clean
```

## 运行

```bash
adb push build/local_api_arm64-v8a /data/local/tmp/local_api
adb shell chmod +x /data/local/tmp/local_api
adb shell /data/local/tmp/local_api
```

> 图标隐藏、SusFS / 应用隐藏配置等功能需要以 root 身份启动本服务（例如在已获取 root 的 shell 中运行）。

## 版本管理

| 常量 | 当前值 | 说明 |
|------|--------|------|
| `SERVER_VERSION` | `1.1.0` | 发版递增 |
| `API_VERSION` | `2.0` | 接口格式变更递增 |

前端调用 `{"action":"version"}` 检查是否需要更新。

## 许可证

GPL-3.0-or-later
