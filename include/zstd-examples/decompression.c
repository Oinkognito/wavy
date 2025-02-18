#include "../decompression.h"

int main(int argc, const char** argv)
{
  const char* const exeName = argv[0];

  if (argc != 2)
  {
    ZSTD_LOG_START_SECTION("Input Error");
    printf("┃ wrong arguments\n");
    printf("┃ usage: %s FILE\n", exeName);
    ZSTD_LOG_END_SECTION();
    return 1;
  }

  if (ZSTD_decompress_file(argv[1]))
    printf("%s correctly decoded (in memory). \n", argv[1]);

  return 0;
}
