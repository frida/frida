# Frida Stealth Build & Deploy Guide

本文档说明如何编译、部署和验证一个反检测优化版本的 Frida，使其能够绕过 RiskEngine 等设备指纹/风控 SDK 的检测。

---

## 目录

1. [环境准备](#环境准备)
2. [快速开始（一键脚本）](#快速开始)
3. [手动编译](#手动编译)
4. [配置选项详解](#配置选项详解)
5. [部署到设备](#部署到设备)
6. [使用方法](#使用方法)
7. [检测验证](#检测验证)
8. [技术原理](#技术原理)
9. [故障排除](#故障排除)

---

## 环境准备

### 系统要求

| 组件 | 最低版本 | 说明 |
|------|---------|------|
| OS | Ubuntu 22.04 / macOS 13+ | 编译宿主机 |
| Python | 3.9+ | 构建脚本 |
| GCC/Clang | 12+ | C/C++ 编译器 |
| Vala | Frida fork | 必须使用 frida/vala 仓库的版本 |
| Node.js | 18+ | 可选，frida-tools |
| Go | 1.24+ | 可选，compiler backend |
| Android NDK | r25+ | 交叉编译 Android 目标 |

### 一键安装依赖（Ubuntu）

```bash
# 基础工具
sudo apt update && sudo apt install -y \
    build-essential python3 python3-pip ninja-build \
    git curl wget pkg-config flex bison \
    libglib2.0-dev libgirepository1.0-dev

# Android NDK（如果交叉编译 Android）
# 方式 1: 通过 sdkmanager
sdkmanager "ndk;25.2.9519653"
export ANDROID_NDK_ROOT=$HOME/Android/Sdk/ndk/25.2.9519653

# 方式 2: 直接下载
wget https://dl.google.com/android/repository/android-ndk-r25c-linux.zip
unzip android-ndk-r25c-linux.zip
export ANDROID_NDK_ROOT=$PWD/android-ndk-r25c
```

### 克隆仓库

```bash
git clone --recurse-submodules https://github.com/<your-fork>/frida.git
cd frida
```

---

## 快速开始

提供了一键编译和部署脚本，无需手动配置参数。

### 编译

```bash
# 全隐身模式编译 Android ARM64
./tools/build-stealth.sh android-arm64 full

# 最小改动模式
./tools/build-stealth.sh android-arm64 minimal

# 仅 Gadget 模式（最隐蔽）
./tools/build-stealth.sh android-arm64 gadget

# 自定义配置
cp tools/stealth.conf.example tools/stealth.conf
# 编辑 tools/stealth.conf
./tools/build-stealth.sh android-arm64 custom
```

### 部署

```bash
# 一键推送 + 启动 + 验证
./tools/deploy-stealth.sh all

# 分步操作
./tools/deploy-stealth.sh push     # 推送到设备
./tools/deploy-stealth.sh start    # 启动 server
./tools/deploy-stealth.sh verify   # 运行检测验证
./tools/deploy-stealth.sh stop     # 停止 server
./tools/deploy-stealth.sh clean    # 清理设备
```

### 预设说明

| 预设 | 适用场景 | 隐蔽程度 |
|------|---------|---------|
| `full` | 需要 frida-server 的完整功能 | 高 |
| `minimal` | 快速测试，不需要协议伪装 | 中 |
| `gadget` | 无 root 环境，注入 APK | 最高 |
| `custom` | 精确控制每个参数 | 自定义 |

---

## 手动编译

如果不使用脚本，可以手动配置和编译。

### Android ARM64（最常见）

```bash
./configure \
  --host=android-arm64 \
  -Dfrida-gum:stealth_memfd_name=jit-cache \
  -Dfrida-gum:stealth_thread_js="Signal Catcher" \
  -Dfrida-core:stealth_server_name=media.codec \
  -Dfrida-core:stealth_helper_name=media.extractor \
  -Dfrida-core:stealth_gadget_name=libhwui \
  -Dfrida-core:stealth_port=52173 \
  -Dfrida-core:stealth_thread_main="HwBinder:1" \
  -Dfrida-core:stealth_thread_gadget=RenderThread \
  -Dfrida-core:stealth_server_dir=com.android.providers.media \
  -Dfrida-core:stealth_magic=deadbeefcafebabe

make -j$(nproc)
```

### Android ARM (32-bit)

```bash
./configure --host=android-arm \
  -Dfrida-gum:stealth_memfd_name=jit-cache \
  -Dfrida-core:stealth_server_name=media.codec \
  -Dfrida-core:stealth_port=52173

make -j$(nproc)
```

### Linux x86_64（本机测试）

```bash
./configure \
  -Dfrida-gum:stealth_memfd_name=jit-cache \
  -Dfrida-core:stealth_port=52173

make -j$(nproc)
```

### 编译产物位置

```
build/
├── subprojects/frida-core/
│   ├── server/<server_name>          # frida-server
│   ├── inject/frida-inject           # frida-inject
│   └── lib/
│       ├── gadget/lib<gadget_name>.so  # frida-gadget
│       └── agent/frida-agent.so        # agent (内部使用)
└── subprojects/frida-gum/
    └── ...
```

---

## 配置选项详解

### frida-gum 选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `stealth_memfd_name` | string | `jit-cache` | memfd_create 名称，出现在 /proc/self/maps。建议值：`jit-cache`、`boot.oat`、`boot-framework` |
| `stealth_thread_js` | string | `gum-js-loop` | GumJS 事件循环线程名 |

### frida-core 选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `stealth_server_name` | string | `frida-server` | server 二进制输出名 |
| `stealth_helper_name` | string | `frida-helper` | helper 二进制输出名 |
| `stealth_gadget_name` | string | `frida-gadget` | gadget .so 名（不含 lib 前缀和 .so 后缀） |
| `stealth_port` | integer | `27042` | 默认控制端口。cluster 端口 = 此值 + 10 |
| `stealth_thread_main` | string | `frida-main-loop` | frida-core 主循环线程名 |
| `stealth_thread_gadget` | string | `frida-gadget` | gadget worker 线程名 |
| `stealth_server_dir` | string | `re.frida.server` | server 临时目录名 |
| `stealth_magic` | string | _(空)_ | D-Bus 前置认证 magic（十六进制）。空=禁用 |

### 推荐线程名（Android 系统常见）

```
Signal Catcher    # 信号处理线程
FinalizerDaemon   # GC 终结器
ReferenceQueueD   # 引用队列
HeapTaskDaemon    # 堆任务
Binder:xxx        # Binder 线程
RenderThread      # 渲染线程
HwBinder:1        # 硬件 Binder
Jit thread pool   # JIT 编译池
hwuiTask0         # HWUI 任务
```

---

## 部署到设备

### 方式 1: frida-server（需要 root）

```bash
# 1. 推送
adb push build/subprojects/frida-core/server/media.codec /data/local/tmp/
adb shell chmod 755 /data/local/tmp/media.codec

# 2. 启动
adb shell "su -c '/data/local/tmp/media.codec &'"

# 3. 端口转发（如果通过 USB 连接）
adb forward tcp:52173 tcp:52173

# 4. 验证
frida -H 127.0.0.1:52173 -l test.js -n com.target.app
```

### 方式 2: frida-gadget（无需 root，推荐）

```bash
# 1. 解包 APK
apktool d target.apk -o target_dir

# 2. 复制 gadget
cp build/subprojects/frida-core/lib/gadget/libhwui.so \
   target_dir/lib/arm64-v8a/libhwui.so

# 3. 创建配置文件
cat > target_dir/lib/arm64-v8a/libhwui.config.so << 'EOF'
{
  "interaction": {
    "type": "script",
    "path": "/data/local/tmp/hook.js",
    "on_change": "reload"
  }
}
EOF

# 4. 修改入口 Activity 加载 gadget
#    在 smali 的 onCreate 或 <clinit> 中添加:
#    invoke-static {}, Ljava/lang/System;->loadLibrary(Ljava/lang/String;)V
#    const-string v0, "hwui"

# 5. 重新打包签名
apktool b target_dir -o target_patched.apk
zipalign -v 4 target_patched.apk target_aligned.apk
apksigner sign --ks my.keystore target_aligned.apk

# 6. 安装
adb install target_aligned.apk

# 7. 推送脚本
adb push hook.js /data/local/tmp/hook.js
```

### 方式 3: frida-gadget + Listen 模式

```json
{
  "interaction": {
    "type": "listen",
    "address": "127.0.0.1",
    "port": 52173
  }
}
```

然后通过 frida-tools 连接：
```bash
frida -H 127.0.0.1:52173 -n Gadget
```

### 方式 4: frida-inject（需要 root）

```bash
adb push build/subprojects/frida-core/inject/frida-inject /data/local/tmp/injector
adb shell "su -c '/data/local/tmp/injector -p $(pidof com.target.app) -s /data/local/tmp/hook.js'"
```

---

## 使用方法

### 基本 Hook 脚本示例

```javascript
// hook.js - 绕过 RiskEngine 检测的辅助脚本
Java.perform(function() {
    // 统一 Android ID 返回值（对抗多源验证）
    var fakeId = "0123456789abcdef";

    // 路径 1: Settings.Secure.getString
    var Secure = Java.use("android.provider.Settings$Secure");
    Secure.getString.overload("android.content.ContentResolver", "java.lang.String")
        .implementation = function(cr, name) {
            if (name === "android_id") return fakeId;
            return this.getString(cr, name);
        };

    // 路径 2: ContentResolver.call
    var ContentResolver = Java.use("android.content.ContentResolver");
    ContentResolver.call.overload(
        "android.net.Uri", "java.lang.String", "java.lang.String", "android.os.Bundle"
    ).implementation = function(uri, method, arg, extras) {
        if (method === "GET_secure" && arg === "android_id") {
            var bundle = Java.use("android.os.Bundle").$new();
            bundle.putString("value", fakeId);
            return bundle;
        }
        return this.call(uri, method, arg, extras);
    };

    console.log("[*] Android ID hooks installed");
});
```

### 连接方式对照表

| 场景 | 命令 |
|------|------|
| USB + server | `frida -H 127.0.0.1:<port> -n <app>` |
| WiFi + server | `frida -H <device_ip>:<port> -n <app>` |
| Gadget listen | `frida -H 127.0.0.1:<port> -n Gadget` |
| Gadget script | 自动执行，无需连接 |
| Inject | 一次性执行，无需连接 |

---

## 检测验证

### 自动验证（推荐）

```bash
./tools/deploy-stealth.sh verify
```

输出示例：
```
[*] === Detection Verification (PID: 12345) ===

[*] Test 1: /proc/self/maps - frida string
[PASS] No 'frida' string in maps
[*] Test 2: /proc/self/maps - anonymous executable segments
[PASS] No anonymous executable segments
[*] Test 3: /proc/self/maps - memfd segments
[PASS] Found 3 memfd segments (expected)
[*] Test 4: Thread names
[PASS] No detectable thread names
[*] Test 5: Port 27042 (default frida port)
[PASS] Port 27042 not in use
[*] Test 6: Custom port 52173
[PASS] Custom port 52173 is listening (expected)
[*] Test 7: D-Bus protocol probe
[PASS] Got HTTP 404 (magic protection active)
[*] Test 8: Process name in /proc
[PASS] No 'frida' process names

[*] === Verification Complete ===
```

### 手动验证

```bash
# 获取目标进程 PID
PID=$(adb shell "su -c 'pidof media.codec'")

# 1. maps 字符串
adb shell "su -c 'cat /proc/$PID/maps'" | grep -i frida
# 期望: 无输出

# 2. 匿名可执行段
adb shell "su -c 'cat /proc/$PID/maps'" | grep "r-xp" | grep "00:00 0" | grep -v "\["
# 期望: 无输出

# 3. memfd 段
adb shell "su -c 'cat /proc/$PID/maps'" | grep "memfd:"
# 期望: 显示 memfd:jit-cache

# 4. 线程名
for tid in $(adb shell "su -c 'ls /proc/$PID/task/'"); do
    name=$(adb shell "su -c 'cat /proc/$PID/task/$tid/comm'" 2>/dev/null)
    echo "$tid: $name"
done | grep -i "frida\|gum-js\|gmain"
# 期望: 无输出

# 5. 端口
adb shell "su -c 'cat /proc/net/tcp'" | grep $(printf '%04X' 27042)
# 期望: 无输出

# 6. D-Bus 探针
adb shell "su -c 'echo -ne \"\\x00AUTH\\r\\n\" | nc -w 1 127.0.0.1 52173'"
# 期望: HTTP/1.1 404 (如果启用了 magic)
# 或无响应 (连接被关闭)
```

---

## 技术原理

### 对抗层级总览

| 层级 | 检测手段 | 本方案对抗措施 | 信号级别 |
|------|---------|--------------|---------|
| L1 | maps 字符串扫描 | .so 重命名 | weak |
| L1 | 线程名扫描 | 线程名伪装为系统线程 | medium |
| L1 | 端口扫描 | 随机高位端口 | medium |
| L2 | D-Bus 协议探针 | magic 前置认证 + 404 伪装 | strong |
| L3 | 进程名+端口关联 | 进程名伪装 + 端口变更 | strong |
| L4 | 匿名可执行内存 | memfd_create 伪装为 jit-cache | strong |
| L4 | LDR+BR trampoline | ADRP+ADD+BR / MOVZ+MOVK+BR | strong |
| L4 | SIGTRAP 探针 | Frida 不拦截 SI_TKILL 信号 | strong |
| L5 | 方法入口指针检查 | 代码分配到白名单 memfd 区域 | strong |

### RiskEngine 组合判定逻辑

```
if (strong >= 2 || (strong >= 1 && medium >= 2)):
    DEADLY
elif (strong >= 1 || medium >= 2):
    HIGH
else:
    MEDIUM (可接受)
```

本方案目标：**strong = 0, medium <= 1** → 判定结果为 MEDIUM 或更低。

---

## 故障排除

### 编译失败

| 错误 | 原因 | 解决 |
|------|------|------|
| `Unknown option stealth_*` | frida-gum/frida-core 子模块未更新 | `git submodule update --init` |
| `Vala compiler not found` | 未安装 Frida fork 的 Vala | 参考 frida/vala 仓库编译安装 |
| `NDK not found` | 交叉编译缺少 NDK | 设置 `ANDROID_NDK_ROOT` 环境变量 |
| `memfd_create undeclared` | 内核头文件过旧 | 确保 `sys/syscall.h` 包含 `__NR_memfd_create` |

### 运行时问题

| 现象 | 原因 | 解决 |
|------|------|------|
| 客户端连不上 server | magic 不匹配 | 确保客户端和服务端使用相同 magic 编译 |
| 客户端连不上 server | 端口不对 | 使用 `-H 127.0.0.1:<custom_port>` |
| Gadget 不加载 | .so 名不匹配 | 检查 `System.loadLibrary` 参数与文件名一致 |
| Hook 后崩溃 | trampoline 空间不足 | 极少见，可能是目标函数太短 |
| 验证发现匿名段 | 非 Frida 的匿名段 | 检查是否为 ART JIT 或其他合法段 |

### 常见问题

**Q: 不传任何 stealth 选项会怎样？**
A: 所有选项都有默认值，等同于编译原版 Frida。完全向后兼容。

**Q: 只改端口不改其他，有用吗？**
A: 有限。能绕过固定端口扫描，但 maps 字符串、线程名、内存特征仍会暴露。建议至少使用 `minimal` 预设。

**Q: Gadget 模式需要配置 magic 吗？**
A: 如果 Gadget 使用 `script` 交互模式（不开端口），不需要。如果使用 `listen` 模式，建议配置。

**Q: 能否对抗所有检测 SDK？**
A: 本方案针对 RiskEngine 的检测逻辑设计。其他 SDK（如 SecNeo、梆梆、爱加密）可能有不同检测维度，但核心原理相通。内存层优化（memfd + trampoline 变形）对大多数检测方案都有效。

---

## 文件清单

```
tools/
├── build-stealth.sh        # 一键编译脚本
├── deploy-stealth.sh       # 一键部署+验证脚本
├── stealth.conf.example    # 自定义配置模板
docs/
└── STEALTH-BUILD.md        # 本文档
```
