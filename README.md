# Local API

C++17 本地 HTTP 服务。仅开放一个接口 `POST /`，通过请求体 `action` 字段分发工具调用。

> ⚠️ **当前状态：全部工具均为占位（placeholder）**。工具不接收任何参数，服务只做分发，不执行任何功能；Root 检测库已编译但暂不调用。功能实现待后续开发。

## 架构

```
main.cpp      → 引用启动（只 include version.hpp / router.hpp，调 router::run()）
router.cpp    → 请求分发（网卡绑定、解析 action、分发对应工具函数）
tools.cpp     → 工具函数集合（detect / version / debug，全部占位、不传参）
detector.cpp  → 纯检测库工具，暂不调用（KSU / APatch / Magisk / SusFS 握手）
version.hpp   → 版本号 + 配置常量（整合到 main 引用）
```

- 前端 UI 无需 root 即可调用本服务
- 后端功能实现（需要 root 权限）待后续接入

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

### POST / — 唯一入口

```bash
curl -X POST http://127.0.0.1:8080/ -H "Content-Type: application/json" -d '{"action":"功能名"}'
```

| action | 状态 | 说明 |
|---|---|---|
| `detect` | placeholder | Root 检测（KSU / APatch / Magisk / SusFS 握手），未接线 |
| `version` | ok | 服务版本 + API 版本 |
| `debug` | ok | 版本、已注册工具、root 状态 |

- 占位工具统一返回：`{"status":"placeholder","action":"...","message":"not implemented — placeholder only"}`
- 未知 action：`{"status":"error","error":"unknown action: ...","available_actions":[...]}`（HTTP 200）
- 非法/缺失 JSON：HTTP 400

### GET /debug — 服务自省

返回版本信息、已注册工具列表、当前进程是否 root（`is_root`）。


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
