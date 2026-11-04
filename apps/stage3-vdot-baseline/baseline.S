.text
.option push
.option arch, +v
.balign 4
.globl run_vdot_baseline
.type run_vdot_baseline, @function

# 输入：
#   a0：输入 a 的地址，至少 16 字节
#   a1：输入 b 的地址，至少 16 字节
#   a2：输出缓冲区地址，至少 16 字节
#
# 输出：
#   缓冲区低 32 位：低八组有符号 8 位输入的点积
#   缓冲区高 96 位：全部为零
#   返回值 a0：计算阶段的实际 VL，预期为 8
#
# 适用条件：VLEN=128，M 模式裸机环境。
run_vdot_baseline:
  # 将 mstatus.VS 置为 Dirty，启用向量状态。
  li t0, 0x600
  csrs mstatus, t0
  csrw vstart, zero

  # 加载完整的 16 字节输入。
  # 高八个元素也进入源寄存器，用于检查是否错误地多算。
  vsetivli zero, 16, e8, m1, tu, mu
  vle8.v v1, (a0)
  vle8.v v2, (a1)

  # 只计算低八组输入：有符号 8 位 × 8 位 → 16 位。
  # t0 保存计算阶段的实际 VL。
  vsetivli t0, 8, e8, m1, tu, mu
  vwmul.vv v4, v1, v2

  # 乘法目的寄存器组为 v4、v5，但八个有效乘积
  # 总共只有 128 位，全部位于 v4，因此按 e16、m1 读取。
  vsetivli zero, 8, e16, m1, tu, mu

  # 此时八个 16 位元素覆盖整个 v3，完整清零。
  # 低 32 位作为累加初值，高 96 位预置为最终输出的零。
  vmv.v.i v3, 0

  # 将八个乘积按有符号数加宽归约到 32 位。
  # 结果写入 v3[31:0]，tu 使高 96 位保持为零。
  vwredsum.vs v3, v4, v3

  # 按字节保存整个 v3，共 16 字节。
  vsetivli zero, 16, e8, m1, tu, mu
  vse8.v v3, (a2)

  # 通过 a0 返回计算阶段的 VL；点积结果由缓冲区返回。
  mv a0, t0
  ret

.size run_vdot_baseline, .-run_vdot_baseline
.option pop
