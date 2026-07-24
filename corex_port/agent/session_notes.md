# ILGPU → Iluvatar CoreX (ivcore11) 迁移会话记录

## 来源
- 上游仓库：https://github.com/m4rs-mt/ILGPU.git
- 默认分支：`master`
- 起点 commit：`ea51bcbdc3695554b8b9899a225d37c12cd0babe`（2026-04-16）
- 项目性质：ILGPU 是 C#/.NET 的 GPU JIT 编译器。其 CUDA 后端把 .NET IL 编译为 **NVIDIA 风格 PTX 文本**，再通过 CUDA driver API（`cuModuleLoadData` / `cuLaunchKernel`）在运行时 JIT 加载执行。仓库内 **没有 nvcc / .cu 文件**。

## CUDA 使用性质
- 「编译」有两层：
  1. **.NET 构建**（`dotnet build`）——生成 ILGPU 程序集与测试程序集（本次的 compile 面）。
  2. **运行时 PTX JIT**——kernel 在运行时由 ILGPU 生成 PTX，交给 CoreX 驱动 JIT（属运行时，不计入 compile）。
- 不使用 nvcc；未创建任何 nvcc dummy；未触碰 `/usr/local/corex`。

## 环境
- CoreX SDK：`/usr/local/corex`，`ixsmi` 报 CUDA 10.2、驱动 4.5.0，2×Iluvatar BI-V150（ivcore11，Volta 级，上报 cc 7.1，warpSize=64，各 32GB）。
- .NET SDK：仓库 `global.json` 要求 8.0；容器初始无 dotnet，**用户态安装** `dotnet-install.sh --channel 8.0` 到 `/root/.dotnet`（workaround-able，未污染系统）。
- GPU 运行必须在**非 sandbox**下（sandbox 屏蔽 `/dev/iluvatar*`，`cuInit` 会返回 100 NO_DEVICE）。
- 关键环境：`LD_LIBRARY_PATH` 含 `/usr/local/corex/lib(:lib64)`；每次跑 GPU 前 `ixsmi -r`。

## 适配内容（均为仓库内局部改动，zero-touch SDK）
1. **`Src/ILGPU/Runtime/Cuda/CudaDriverVersion.cs`**：`GetMinimumDriverVersion(架构)` 未知架构回退逻辑。原逻辑对未知架构返回「最新已知架构」的最低驱动版本（CUDA 12.x），使上报 cc 7.1 的 CoreX 卡在 CUDA 10.2 驱动上选不出 PTX ISA、被设备 predicate 拒绝（枚举得 0 卡）。改为回退到「**≤ 该架构的最近已知架构**」（SM_71→SM_70→CUDA 9.0），从而在 10.2 驱动上选出 ISA_65，正确识别 2 卡（ISA 6.5、warp 64）。
2. **`Src/ILGPU/Context.Builder.cs`**：`LibDevice` 自动探测健壮化。CoreX 有 `nvvm/libdevice` 但无 `nvvm/lib64`，`Directory.EnumerateFiles` 对缺失目录抛未捕获 `DirectoryNotFoundException` 导致 `Context.Create` 崩溃。加 `Directory.Exists` 判空后按 `throwIfNotFound` 语义优雅跳过（默认 PTX 后端不依赖 NVVM/LibDevice）。
3. **驱动残留处理**：每次 GPU 运行前 `ixsmi -r`（`cuCtxCreate` 残留挂死的规避，见 blockers）。
4. **诊断脚手架**（`corex_port/probe/`）：ILGPU 探针（枚举/建 accelerator/编译并 dump PTX/加载/启动）+ 裸 C 驱动探针（cuInit/ctx/PTX 加载）用于诚实复现每一道墙；不属产品改动。

> 说明：上述 (1)(2) 让 ILGPU 能在 ivcore11 上**识别设备、建上下文、生成 PTX**；但见下方 terminal 墙，kernel 无法真正在 GPU 上执行。

## 结果
- **.NET 构建（compile_status）**：success。`dotnet build ILGPU.sln -c Release` 全绿（0 error / 0 warning），16 个工程含 ILGPU.dll、ILGPU.Algorithms.dll、全部测试程序集。
- **运行时测试（test_status）**：partial_pass。
  - **CUDA 套件（迁移目标，主计数）**：`ILGPU.Tests.Cuda` 共 **37999** 例：**186 通过 / 37795 失败 / 18 跳过**。186 通过为非 kernel 用例（设备/加速器属性、能力查询、部分 host/分配路径）；37795 全部因同一 terminal 墙失败。
  - **CPU 基线（单列 scope，不计入主计数）**：`ILGPU.Tests.CPU` 共 **21127** 例：**20920 通过 / 0 失败 / 207 跳过**。证明构建、库本体与测试框架健康，问题被隔离到 GPU PTX 路径。
- **overall_status：blocked** —— 存在未解决的 terminal blocker（驱动拒绝 NV PTX），CUDA 后端在 ivcore11 上无法执行 kernel。

## Failure Gate（逐墙真实复现 + 分类 + 已尝试的 workaround）
1. **设备枚举得 0**（workaround-able，已解）：插桩定位为未知 cc 7.1 → ISA 选不出 → predicate 拒绝；改回退逻辑后解决。
2. **LibDevice 崩溃**（workaround-able，已解）：真实复现 `DirectoryNotFoundException`；加目录判空解决。
3. **cuCtxCreate 挂死**（workaround-able，已解）：`timeout` 抓到 90s 挂死；`ixsmi -r` 后 0.01s 成功。
4. **cuModuleLoadData 拒绝 NV PTX（terminal）**：真实复现 —— dump 出 ILGPU 的 PTX（`.target sm_71/.version 6.5`），driver 返回 `CUDA_ERROR_INVALID_IMAGE(200)`、JIT log 空白；进一步用裸 C 尝试**改 target 到 sm_70**、以及**手写最小 PTX**跨 version 6.0/6.5/7.0 × target sm_62/70/72，`cuModuleLoadData`/`cuModuleLoadDataEx` **全部 rc=200**。对照 iluvatar-cuda-base 兼容索引确认为已知平台边界「ivcore11 不消费 NV PTX 文本」。因红线不可改驱动、且需为 ILGPU 新增 ivcore11 原生后端方能绕过（超出仓库局部适配范围），判定为 **terminal**，未再强行绕过。

## 复现命令要点
```
source /home/init_container.sh
export DOTNET_ROOT=/root/.dotnet PATH=$PATH:/root/.dotnet
export LD_LIBRARY_PATH=/usr/local/corex/lib:/usr/local/corex/lib64:$LD_LIBRARY_PATH
export CUDA_HOME=/usr/local/corex
cd Src && dotnet build ILGPU.sln -c Release
ixsmi -r
dotnet test ILGPU.Tests.Cuda/ILGPU.Tests.Cuda.csproj -c Release --no-build   # 主：CUDA 套件
dotnet test ILGPU.Tests.CPU/ILGPU.Tests.CPU.csproj   -c Release --no-build   # 基线：CPU 套件
# 裸驱动复现见 corex_port/probe/ptx_load_probe.c
```
