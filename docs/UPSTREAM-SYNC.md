# 上游同步方案

## 架构

```
frida/frida (upstream)
    │
    ▼  (自动/手动 sync)
HaiChecker/frida (fork - main)
    │
    ├── feature/stealth-anti-detection  (stealth 补丁分支)
    └── main                            (跟踪上游 + stealth 合并)
```

## 自动同步（GitHub Actions）

已配置 `.github/workflows/sync-upstream.yml`，行为如下：

| 触发条件 | 行为 |
|---------|------|
| 每天 UTC 8:00 | 自动 fetch upstream/main 并尝试合并 |
| 手动 dispatch | 在 Actions 页面点击 "Run workflow" |
| 无冲突 | 直接合并并 push 到 main |
| 有冲突 | 创建 PR（标签 `upstream-sync`），等待手动解决 |

### 启用步骤

1. 推送 `.github/workflows/sync-upstream.yml` 到你的 fork
2. 在 fork 的 Settings → Actions → General 中确保 "Allow all actions" 已启用
3. 确保 `GITHUB_TOKEN` 有 `contents: write` 和 `pull-requests: write` 权限（默认已有）

### 手动触发

GitHub → Actions → "Sync Upstream Frida" → Run workflow

---

## 手动同步（本地操作）

当自动同步产生冲突时，或你想精确控制合并过程：

```bash
# 1. 获取上游最新
git fetch upstream

# 2. 查看差距
git log --oneline HEAD..upstream/main | head -20

# 3. 合并（推荐 merge，保留历史）
git merge upstream/main

# 4. 如果有冲突，解决后
git add .
git commit

# 5. 推送
git push origin main
```

### 冲突解决优先级

当上游修改了我们也修改过的文件时：

| 文件 | 策略 |
|------|------|
| `gum/backend-posix/gummemory-posix.c` | 保留我们的 memfd 逻辑，合并上游的其他改动 |
| `gum/backend-arm64/guminterceptor-arm64.c` | 保留我们的 trampoline 变形，合并上游的 bug fix |
| `gum/arch-arm64/gumarm64writer.c` | 保留我们新增的 MOVZ/MOVK 函数，合并上游新增的其他函数 |
| `src/control-service.vala` | 保留我们的 magic 认证逻辑，合并上游的其他改动 |
| `meson.options` | 追加合并（我们的选项在文件末尾，不太会冲突） |
| `meson.build` | 需要仔细看上游改了什么，手动合并 |

### 子模块同步

frida 主仓库通过 git submodule 引用 frida-gum 和 frida-core。如果你 fork 了子模块：

```bash
# 更新子模块指向
cd subprojects/frida-gum
git fetch upstream
git merge upstream/main
cd ../..

cd subprojects/frida-core
git fetch upstream
git merge upstream/main
cd ../..

# 更新主仓库的子模块引用
git add subprojects/frida-gum subprojects/frida-core
git commit -m "submodules: sync upstream + stealth patches"
```

如果你没有 fork 子模块（直接在主仓库的 subprojects 目录下工作），则子模块变更需要通过 patch 文件管理。

---

## 版本兼容性检查

上游更新后，需要验证 stealth 补丁仍然有效：

```bash
# 编译测试
./tools/build-stealth.sh android-arm64 full

# 如果编译失败，常见原因：
# 1. 上游改了 gum_allocate_page_aligned 的签名 → 更新 memfd 逻辑
# 2. 上游改了 interceptor 的 trampoline 流程 → 更新 activate_trampoline
# 3. 上游改了 control-service 的连接处理 → 更新 magic 认证位置
# 4. 上游新增了 meson option 导致冲突 → 重新追加我们的选项
```

---

## CI 验证

已配置 `.github/workflows/stealth-ci.yml`：

- 每次 push 到 main 或 stealth 分支时自动运行
- 验证所有 stealth meson option 存在
- 验证关键代码修改点完整
- 验证脚本语法正确

---

## 推荐工作流

```
1. 日常开发在 feature/stealth-* 分支
2. 完成后 merge 到 main
3. GitHub Actions 每天自动同步上游
4. 无冲突 → 自动合并
5. 有冲突 → 收到 PR 通知 → 本地解决 → push
6. 解决后运行 ./tools/build-stealth.sh 验证
```
