# 香山 vdot 自定义指令与协同仿真

本项目面向 CIE 香山赛道，实现固定八元素的有符号向量点积指令 `vdot.vv`，并完成 NEMU 参考模型扩展、RTL/NEMU 差分验证和标准向量指令基线对比。

仓库保存源码补丁、测试程序和复现说明。使用者需要先准备可用的 xs-env 环境；完整香山工程和编译产物不包含在本仓库中。

## 指令约定

```c
int32_t sum = 0;
for (int i = 0; i < 8; i++)
  sum += (int32_t)(int8_t)vs1[i] * (int32_t)(int8_t)vs2[i];
```

- 两个源寄存器只取低八个有符号 8 位元素，不累加旧目标值。
- 结果以 32 位补码写入 `vd[31:0]`，`vd[127:32]` 清零。
- 当前仅支持 `e8、m1、vl=8、vstart=0、vm=1`；不支持的配置按非法指令处理。
- 当前编码属于 OPMVV：`funct6=001110、funct3=010、opcode=1010111`。
- 测试使用 `.word 0x3a1121d7`，表示 `vdot.vv v3, v1, v2`。该机器码只对应这组寄存器，汇编器未增加通用助记符支持。

当前汇编测试会分别从两个输入地址加载 16 字节，再以 `vl=8` 计算。输入数组和输出缓冲区均须至少有 16 字节，测试中采用 16 字节对齐。这个接口服务于固定八元素实验。

## 仓库内容

| 路径 | 内容 |
| --- | --- |
| `patches/xiangshan-vdot.patch` | 香山译码、功能单元接入及结果处理 |
| `patches/yunsuan-vdot.patch` | YunSuan 操作类型与乘法结果通路修改 |
| `patches/nemu-vdot.patch` | 指令登记、OPMVV 译码及参考执行语义 |
| `apps/` | 正数、负数、边界、标准基线和性能测试 |
| `versions/` | 上游基线记录 |
| `configs/nemu.config` | 作者导出的 NEMU 构建配置 |
| `docs/`、`logs/` | 已整理的报告与实际运行记录，随验证进度补充 |

## 基线与环境

三个补丁分别针对下列提交生成，不能只应用其中一份就得到完整实现。

| 工程 | 基线提交 |
| --- | --- |
| XiangShan | `16ae9ddcda54fc9a2fddffad73174cf793ac7814` |
| YunSuan | `cadd3c2f43096e253d61296b33ac697be8354e29` |
| NEMU | `53bcb5686f8fd05248ae98546b7dc04bdca1bbb0` |
| nexus-am | 见 `versions/nexus-am-commit.txt` |

运行目标为 RV64、VLEN=128 的香山裸机仿真环境。AM 使用 `ARCH=riscv64-xs`，交叉工具链前缀为 `riscv64-linux-gnu-`。其他依赖和子模块按对应版本的上游工程准备。

以下操作适用于独立、干净且处于对应基线的工作副本。已经加入 vdot 的开发目录不要重复应用补丁。

## 复现

### 1. 设置路径

先进入本仓库根目录。将下面三个 `/path/to/xs-env` 替换为实际位置；ZIP 解压目录名称不影响操作。

```bash
export VDOT_HOME="$PWD"
export NOOP_HOME=/path/to/xs-env/XiangShan
export NEMU_HOME=/path/to/xs-env/NEMU
export AM_HOME=/path/to/xs-env/nexus-am
```

### 2. 检查并应用补丁

先检查三份补丁，三项均无报错后再执行应用操作：

```bash
git -C "$NOOP_HOME" apply --check "$VDOT_HOME/patches/xiangshan-vdot.patch" &&
git -C "$NOOP_HOME/yunsuan" apply --check "$VDOT_HOME/patches/yunsuan-vdot.patch" &&
git -C "$NEMU_HOME" apply --check "$VDOT_HOME/patches/nemu-vdot.patch"
```

```bash
git -C "$NOOP_HOME" apply "$VDOT_HOME/patches/xiangshan-vdot.patch" &&
git -C "$NOOP_HOME/yunsuan" apply "$VDOT_HOME/patches/yunsuan-vdot.patch" &&
git -C "$NEMU_HOME" apply "$VDOT_HOME/patches/nemu-vdot.patch"
```

YunSuan 是独立的 Git 仓库，因此必须单独应用其补丁。随后复制测试应用：

```bash
cp -r "$VDOT_HOME/apps/." "$AM_HOME/apps/"
```

### 3. 构建 NEMU 和 RTL 仿真器

将保存的配置作为 NEMU 的一个 defconfig 导入，再构建参考动态库：

```bash
cp "$VDOT_HOME/configs/nemu.config" "$NEMU_HOME/configs/vdot_defconfig" &&
make -C "$NEMU_HOME" vdot_defconfig &&
make -C "$NEMU_HOME" -j1
```

预期生成 `$NEMU_HOME/build/riscv64-nemu-interpreter-so`。参考模型需启用 RVV 和共享库支持，不能使用尚未包含新指令的旧动态库。

下面给出新环境的功能验证构建示例，采用 `MinimalConfig`，关闭波形追踪：

```bash
make -C "$NOOP_HOME" emu \
  CONFIG=MinimalConfig NUM_CORES=1 \
  JVM_XMX=4G EMU_TRACE=0 NO_DIFF=0 -j1
```

预期生成 `$NOOP_HOME/build/emu`。JVM 堆大小和并发数需结合机器资源调整；`4G` 是示例设置。请记录实际 CONFIG、工具版本及构建命令，不同配置的周期结果可能不同。已有其他配置的 RTL 或仿真器时，建议在独立工作副本中重新生成，避免混用旧产物。

### 4. 编译测试并运行差分

以正数测试为例，在 Bash 中执行：

```bash
app=stage2-vdot-positive
mkdir -p "$VDOT_HOME/logs"
set -o pipefail

make -C "$AM_HOME/apps/$app" \
  ARCH=riscv64-xs CROSS_COMPILE=riscv64-linux-gnu- &&
"$NOOP_HOME/build/emu" \
  -i "$AM_HOME/apps/$app/build/$app-riscv64-xs.bin" \
  --diff="$NEMU_HOME/build/riscv64-nemu-interpreter-so" \
  2>&1 | tee "$VDOT_HOME/logs/$app-diff.log"

status=$?
printf '构建/运行退出码：%s\n' "$status"
```

其余测试修改 `app` 后重复本节命令：

| 应用名 | 检查内容 |
| --- | --- |
| `stage2-vdot-positive` | 正数点积与高位清零 |
| `stage2-vdot-negative` | 有符号输入处理与结果格式 |
| `stage2-vdot-boundary` | 零、极值和抵消等边界情况 |
| `stage3-vdot-baseline` | 标准 RVV 指令组合的功能 |
| `stage3-vdot-perf` | 两种实现的结果自检与周期采样 |

通过时应同时看到正确的 reference model 路径、`Difftest enabled`、程序的 `PASS`、正常结束的 `HIT GOOD TRAP`，以及退出码 0。

仅有 GOOD TRAP 或退出码 0 不足以证明测试通过。若出现 `CRITICAL ERROR`、`FAIL`、`ABORT` 或差分不一致，需要先解决异常。

## 标准指令基线

基线先清零完整目标寄存器，再通过有符号加宽乘法和加宽归约求和实现相同结果：

```asm
# 进入此段前：vl=16、e8、m1，v1/v2 已加载输入。
vmv.v.i v3, 0
vsetivli t0, 8, e8, m1, tu, mu
vwmul.vv v4, v1, v2
vsetivli t1, 8, e16, m2, tu, mu
vwredsum.vs v3, v4, v3
```

八对 int8 先得到八个 int16 乘积，再归约为 int32。目标其余位通过预先清零和 tail-undisturbed 策略保留为零。

## 验证与性能记录

作者本地已完成正数、负数、边界、标准基线及性能程序的 RTL/NEMU 差分运行。当前 OPMVV 编码的一次性能运行中，预热两轮后采样八次，结果均为：

| 测量对象 | 计时区间周期数 |
| --- | ---: |
| empty | 59 |
| vdot | 76 |
| baseline | 99 |

两种实现的输出检查通过，程序正常结束。相同计时框架下，直接比值为 `99 / 76 ≈ 1.30`，周期数减少 `23 / 99 ≈ 23.2%`。

若近似认为空框架开销可独立相减，则得到 `vdot=17`、`baseline=40` 周期，对应约 `2.35` 倍。这只是扣除空框架后的辅助估计。

计时区间包含配置、计算和部分 fence 开销；输入加载与结果保存位于区间之外。这些数值是本次实验计时区间的结果，不能直接解释为功能单元的内部延迟。`fence iorw, iorw` 也不应被当成所有处理器上通用的纯算术串行化屏障。周期来源是 RTL 仿真中的计数器，NEMU 用于检查架构状态一致性。

实际记录请保留当前 OPMVV 运行对应的日志。更换核配置、工具链或测试布局后，应重新测量。

## 后续工作

- 补充真实 RTL 波形截图、指令身份对应及输入到输出的延迟分析。
- 扩充寄存器重叠、背靠背指令和非法配置等专项验证。
- 固化构建配置与运行脚本，完善完整日志和报告的归档。

## 来源

本项目基于 [XiangShan](https://github.com/OpenXiangShan/XiangShan)、[YunSuan](https://github.com/OpenXiangShan/YunSuan) 和 [NEMU](https://github.com/OpenXiangShan/NEMU)，测试使用 nexus-am。修改及再分发相关代码时，需遵循对应上游项目的许可证并保留版权声明。

CSR 访问及 FENCE 顺序语义参考 [RISC-V Zicsr：CSR Access Ordering](https://docs.riscv.org/reference/isa/v20260120/unpriv/zicsr.html)。

