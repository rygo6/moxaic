#include <stdio.h>
#include <stdint.h>

static uint32_t CalcDJB2(const char *str, int max) {
  char c; int i = 0; uint32_t hash = 5381;
  while ((c = *str++) && i++ < max)
    hash = ((hash << 5) + hash) + c;
  return hash;
}

int main(int argc, char *argv[]) {
  const char *input = argv[1];
  uint32_t hash = CalcDJB2(input, 256);
  printf("%u\n", hash);
  return 0;
}