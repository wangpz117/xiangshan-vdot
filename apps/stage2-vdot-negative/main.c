#include <am.h>
#include <klib.h>
#include <stdint.h>

extern unsigned long run_vdot(const int8_t *a, const int8_t *b,
                              uint8_t *out);

// 低八个元素包含不同符号组合；高八个元素用于检查是否错误地多算。
static const int8_t a[16] __attribute__((aligned(16))) = {
  -1, 2, -3, 4, -5, 6, -7, 8,
  127, 127, 127, 127, 127, 127, 127, 127
};

static const int8_t b[16] __attribute__((aligned(16))) = {
  9, 10, -11, -12, 13, -14, -15, 16,
  127, 127, 127, 127, 127, 127, 127, 127
};

static uint8_t out[16] __attribute__((aligned(16)));

int main(const char *args) {
  (void)args;
  printf("stage2 negative vdot test start\n");

  // 用普通 C 运算计算期望结果。
  int32_t expected = 0;
  for (int i = 0; i < 8; i++) {
    expected += (int32_t)a[i] * (int32_t)b[i];
  }

  // 预填非零值，防止未写入的位置原本就是零。
  for (int i = 0; i < 16; i++) out[i] = 0xa5;

  unsigned long vl = run_vdot(a, b, out);
  if (vl != 8) {
    printf("FAIL: actual vl = %d, expected 8\n", (int)vl);
    return 1;
  }

  // 按小端顺序，将低四个字节还原成 32 位结果。
  uint32_t actual = (uint32_t)out[0] |
                    ((uint32_t)out[1] << 8) |
                    ((uint32_t)out[2] << 16) |
                    ((uint32_t)out[3] << 24);

  printf("dot result = %d, expected %d\n",
         (int)actual, (int)expected);

  if (actual != (uint32_t)expected) {
    printf("FAIL: dot result mismatch\n");
    return 1;
  }

  // 点积结果之外的十二个字节必须全部为零。
  for (int i = 4; i < 16; i++) {
    if (out[i] != 0) {
      printf("FAIL: out byte %d = %d, expected 0\n",
             i, (int)out[i]);
      return 1;
    }
  }

  printf("PASS: vdot.vv negative and upper bits zero\n");
  return 0;
}