#include <am.h>
#include <klib.h>
#include <stdint.h>

extern unsigned long run_vdot(const int8_t *a, const int8_t *b,
                              uint8_t *out);

static int8_t a[16] __attribute__((aligned(16)));
static int8_t b[16] __attribute__((aligned(16)));
static uint8_t out[16] __attribute__((aligned(16)));

// 返回 0 表示通过，返回 1 表示失败。
// alternate 为真时，a 的低八个元素交替取 av、-av。
static int run_case(const char *name, int8_t av, int8_t bv,
                    int alternate, int32_t expected) {
  int32_t reference = 0;
  for (int i = 0; i < 8; i++) {
    a[i] = (alternate && (i & 1)) ? -av : av;
    b[i] = bv;
    reference += (int32_t)a[i] * (int32_t)b[i];
  }

  // 高八个输入放非零值，检查它们是否被错误地参与计算。
  for (int i = 8; i < 16; i++) {
    a[i] = 127;
    b[i] = 127;
  }
  for (int i = 0; i < 16; i++) out[i] = 0xa5;

  // 检查测试数据与事先计算的期望值是否一致。
  if (reference != expected) {
    printf("FAIL: %s reference mismatch\n", name);
    return 1;
  }

  unsigned long vl = run_vdot(a, b, out);
  if (vl != 8) {
    printf("FAIL: %s actual vl = %d, expected 8\n", name, (int)vl);
    return 1;
  }

  uint32_t actual = (uint32_t)out[0] |
                    ((uint32_t)out[1] << 8) |
                    ((uint32_t)out[2] << 16) |
                    ((uint32_t)out[3] << 24);

  printf("case %s: result = %d, expected %d\n",
         name, (int)actual, (int)expected);

  if (actual != (uint32_t)expected) {
    printf("FAIL: %s dot result mismatch\n", name);
    return 1;
  }

  for (int i = 4; i < 16; i++) {
    if (out[i] != 0) {
      printf("FAIL: %s out byte %d = %d, expected 0\n",
             name, i, (int)out[i]);
      return 1;
    }
  }

  printf("PASS: %s and upper bits zero\n", name);
  return 0;
}

int main(const char *args) {
  (void)args;
  printf("stage2 boundary vdot test start\n");

  if (run_case("zero",        0,    0, 0,       0)) return 1;
  if (run_case("zero-left",   0, -128, 0,       0)) return 1;
  if (run_case("zero-right",127,    0, 0,       0)) return 1;
  if (run_case("max-max",   127,  127, 0,  129032)) return 1;
  if (run_case("min-min",  -128, -128, 0,  131072)) return 1;
  if (run_case("min-max",  -128,  127, 0, -130048)) return 1;
  if (run_case("max-min",   127, -128, 0, -130048)) return 1;
  if (run_case("cancel",    127,  127, 1,       0)) return 1;

  printf("PASS: vdot.vv boundary all cases\n");
  return 0;
}