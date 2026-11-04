.text
.option push
# 仅在这个范围内允许使用向量指令。
.option arch, +v
.balign 4
.globl run_vdot
.type run_vdot, @function

# a0: 输入 a 的地址，至少 16 字节
# a1: 输入 b 的地址，至少 16 字节
# a2: 输出缓冲区地址，至少 16 字节
# 返回值 a0: 点积执行时的 vl
run_vdot:
  # 与阶段一测试一致，启用向量状态。
  li t0, 0x600
  csrs mstatus, t0
  csrw vstart, zero

  # 装入整个源寄存器，高八个字节也放数据，检查是否被错误参与计算。
  vsetivli t0, 16, e8, m1, tu, mu
  vle8.v v1, (a0)
  vle8.v v2, (a1)

  # 把旧目标的所有位填成 1，检查点积是否主动清零高位。
  vmv.v.i v3, -1

  # 使用我们约定的点积配置。
  vsetivli t0, 8, e8, m1, tu, mu

.globl stage2_vdot_insn
stage2_vdot_insn:
  # 自定义 vdot.vv v3, v1, v2。
  # 现有汇编器不认识这个助记符，所以直接写机器码。
  .word 0x3a1121d7

  # 改变配置不会改写 v3；把整个目标寄存器存回内存。
  vsetivli t1, 16, e8, m1, tu, mu
  vse8.v v3, (a2)

  mv a0, t0
  ret
.size run_vdot, .-run_vdot
.option pop