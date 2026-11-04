.text
.option push
.option arch, +v

# 展开成每个测量函数相同的准备部分。
.macro BENCH_BEGIN name
.balign 64
.globl \name
.type \name, @function
\name:
  # a0: 输入 a 地址
  # a1: 输入 b 地址
  # a2: 输出缓冲区地址
  li t0, 0x600
  csrs mstatus, t0
  csrw vstart, zero

  # 准备完整的两个输入寄存器。
  vsetivli t0, 16, e8, m1, tu, mu
  vle8.v v1, (a0)
  vle8.v v2, (a1)

  # 旧目标全部填 1，两种实现从相同状态开始。
  vmv.v.i v3, -1

  # 准备操作完成后，再开始计时。
  fence iorw, iorw
  rdcycle t2
  fence iorw, iorw
.endm

# 展开成每个函数相同的结束部分。
.macro BENCH_END name
  # 等待待测计算完成，再读取结束计数。
  fence iorw, iorw
  rdcycle t3
  fence iorw, iorw

  # 保存结果放在结束计数之后。
  vsetivli t0, 16, e8, m1, tu, mu
  vse8.v v3, (a2)

  # 这些新函数返回周期差，计算结果仍由 out 保存。
  sub a0, t3, t2
  ret
.size \name, .-\name
.endm


# 测量一：空计时框架。
BENCH_BEGIN bench_empty
BENCH_END bench_empty


# 测量二：自定义点积。
BENCH_BEGIN bench_vdot
  vsetivli t0, 8, e8, m1, tu, mu

  # vdot.vv v3, v1, v2，使用你当前的新编码。
  .word 0x3a1121d7
BENCH_END bench_vdot


# 测量三：标准向量指令基线。
BENCH_BEGIN bench_baseline
  # 当前 vl=16，清零完整的 128 位目标寄存器。
  vmv.v.i v3, 0

  # 八对有符号 8 位输入，生成 16 位乘积。
  vsetivli t0, 8, e8, m1, tu, mu
  vwmul.vv v4, v1, v2

  # 将八个 16 位乘积归约为一个 32 位结果。
  vsetivli t1, 8, e16, m2, tu, mu
  vwredsum.vs v3, v4, v3
BENCH_END bench_baseline

.option pop
