# OpenHarmony 官方源码改动补丁

本目录存放对 OpenHarmony **官方源码仓**（非 device/vendor 适配仓）的必要改动补丁，
用于 ingchips 芯片适配。这些改动无法 push 到 device 仓库，以 patch 形式归档，
重装环境时可用 `git apply` 重新应用。

## 补丁列表

| 补丁 | 目标仓 | 内容 | 原因 |
| --- | --- | --- | --- |
| `liteos_m-cortex-m3-gcc.patch` | kernel/liteos_m | 新增 `arch/arm/cortex-m3/gcc/`（复制自 cortex-m4/gcc，汇编无 FPU 指令） | liteos_m 官方 cortex-m3 仅支持 keil，ing20 系列（Cortex-M3）需 gcc 编译 |
| `build-kernel-permission.patch` | build | `kernel_permission.py`：llvm-objcopy 不存在时跳过 kernel permission 处理 | 6.1 mini/liteos_m 使用 gcc 工具链，无 llvm-objcopy |

## 应用方式

```bash
# 进入对应源码仓后
git apply liteos_m-cortex-m3-gcc.patch      # 在 kernel/liteos_m 仓
git apply build-kernel-permission.patch     # 在 build 仓
```
