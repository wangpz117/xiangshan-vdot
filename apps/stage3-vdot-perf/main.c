#include <am.h>
#include <klib.h>
#include <stdint.h>

extern unsigned long bench_empty(const int8_t *, const int8_t *, uint8_t *);
extern unsigned long bench_vdot(const int8_t *, const int8_t *, uint8_t *);
extern unsigned long bench_baseline(const int8_t *, const int8_t *, uint8_t *);

enum { SAMPLES = 8 };

// 低八个元素参与点积，高八个元素用于检查是否错误地多算。
static const int8_t a[16] __attribute__((aligned(16))) = {
  1, 2, 3, 4, 5, 6, 7, 8,
  127, 127, 127, 127, 127, 127, 127, 127
};
static const int8_t b[16] __attribute__((aligned(16))) = {
  10, 10, 10, 10, 10, 10, 10, 10,
  127, 127, 127, 127, 127, 127, 127, 127
};
static uint8_t out[16] __attribute__((aligned(16)));

static int check_output(const char *name, int32_t expected) {
  uint32_t bits = (uint32_t)expected;
  for (int i = 0; i < 16; i++) {
    // 小端格式：前四字节是结果，其余字节应为零。
    uint8_t want = i < 4 ? (uint8_t)(bits >> (8 * i)) : 0;
    if (out[i] != want) {
      printf("FAIL: %s out[%d] = %d, expected %d\n",
             name, i, out[i], want);
      return 1;
    }
  }
  return 0;
}

int main(const char *args) {
  (void)args;
  int32_t expected = 0;
  for (int i = 0; i < 8; i++)
    expected += (int32_t)a[i] * (int32_t)b[i];

  // 三列依次为 empty、vdot、baseline。
  unsigned long cycles[SAMPLES][3];
  unsigned long best[3] = {~0UL, ~0UL, ~0UL};
  const char *names[3] = {"empty", "vdot", "baseline"};

  printf("stage3 vdot performance test start\n");

  // 预热两轮，不计入统计。
  for (int i = 0; i < 2; i++) {
    bench_empty(a, b, out);
    bench_vdot(a, b, out);
    bench_baseline(a, b, out);
  }

  for (int i = 0; i < SAMPLES; i++) {
    cycles[i][0] = bench_empty(a, b, out);

    memset(out, 0xa5, sizeof out);
    cycles[i][1] = bench_vdot(a, b, out);
    if (check_output("vdot", expected)) return 1;

    memset(out, 0xa5, sizeof out);
    cycles[i][2] = bench_baseline(a, b, out);
    if (check_output("baseline", expected)) return 1;
  }

// 采样结束后，统一统计和打印。
for (int i = 0; i < SAMPLES; i++) {
  for (int j = 0; j < 3; j++) {
    if (cycles[i][j] < best[j])
      best[j] = cycles[i][j];
  }

  printf("sample %d: empty=%d, vdot=%d, baseline=%d\n",
         i, (int)cycles[i][0],
         (int)cycles[i][1], (int)cycles[i][2]);
}

for (int j = 0; j < 3; j++)
  printf("min %s = %d\n", names[j], (int)best[j]);

printf("PASS: both implementations produce expected output\n");
return 0;
}