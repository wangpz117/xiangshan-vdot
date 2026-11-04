#include <am.h>
#include <klib.h>
#include <stdint.h>

extern unsigned long run_vdot_baseline(
    const int8_t *a, const int8_t *b, uint8_t *out);

// 每组只列出参与点积的八个元素。
typedef struct {
  const char *name;
  int8_t a[8];
  int8_t b[8];
} TestCase;

static const TestCase tests[] = {
  {"positive",                              // 期望 360
   {1, 2, 3, 4, 5, 6, 7, 8},
   {10, 10, 10, 10, 10, 10, 10, 10}},
  {"mixed",                                 // 期望 -36
   {1, -2, 3, -4, 5, -6, 7, -8},
   {1, 2, 3, 4, 5, 6, 7, 8}},
  {"zero",                                  // 期望 0
   {0, 0, 0, 0, 0, 0, 0, 0},
   {1, 2, 3, 4, 5, 6, 7, 8}},
  {"max_sum",                               // 期望 131072
   {-128, -128, -128, -128, -128, -128, -128, -128},
   {-128, -128, -128, -128, -128, -128, -128, -128}},
  {"min_sum",                               // 期望 -130048
   {-128, -128, -128, -128, -128, -128, -128, -128},
   {127, 127, 127, 127, 127, 127, 127, 127}}
};

// 仍保留 16 字节输入，兼容原来加载 16 个元素的汇编。
static int8_t a[16] __attribute__((aligned(16)));
static int8_t b[16] __attribute__((aligned(16)));
static uint8_t out[16] __attribute__((aligned(16)));

int main(const char *args) {
  (void)args;
  const int count = (int)(sizeof(tests) / sizeof(tests[0]));
  printf("stage3 baseline functional test start\n");

  for (int t = 0; t < count; t++) {
    // 低八项放有效数据，高八项放干扰数据；输出预填非零值。
    for (int i = 0; i < 16; i++) {
      a[i] = i < 8 ? tests[t].a[i] : 127;
      b[i] = i < 8 ? tests[t].b[i] : 127;
      out[i] = 0xa5;
    }

    // 独立用 C 计算期望结果，八对 int8 的点积不会溢出 int32。
    int32_t expected = 0;
    for (int i = 0; i < 8; i++) {
      expected += (int32_t)tests[t].a[i] * (int32_t)tests[t].b[i];
    }

    // 返回值是计算阶段的 vl；结果通过 out 返回。
    unsigned long vl = run_vdot_baseline(a, b, out);
    if (vl != 8) {
      printf("FAIL [%s]: returned vl = %d, expected 8\n",
             tests[t].name, (int)vl);
      return 1;
    }

    // 按小端顺序还原低 32 位，先转为 uint32_t 再移位。
    uint32_t actual = (uint32_t)out[0] |
                      ((uint32_t)out[1] << 8) |
                      ((uint32_t)out[2] << 16) |
                      ((uint32_t)out[3] << 24);

    // 比较 32 位位模式，正数和负数都适用。
    if (actual != (uint32_t)expected) {
      printf("FAIL [%s]: actual=0x%x, expected=0x%x\n",
             tests[t].name, (unsigned int)actual,
             (unsigned int)(uint32_t)expected);
      return 1;
    }

    // 包括负数结果在内，高十二个字节也必须清零。
    for (int i = 4; i < 16; i++) {
      if (out[i] != 0) {
        printf("FAIL [%s]: out[%d] = %d, expected 0\n",
               tests[t].name, i, (int)out[i]);
        return 1;
      }
    }

    printf("PASS [%s]: dot = %d, upper bytes = 0\n",
           tests[t].name, (int)expected);
  }

  printf("PASS: all %d baseline cases\n", count);
  return 0;
}