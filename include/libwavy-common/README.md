# **LIBWAVY-COMMON**

Contains the common utilities of the entire project:

1. `common.h`: For `ZSTD` compression and decompression and main C FFI source.
2. `logger.hpp`: The entire project's logging source with Boost logs
3. `macros.hpp`: Self explanatory all global macros are defined here
4. `state.hpp`: For now just has the global struct of transport segments for decoding audio without file I/O.
