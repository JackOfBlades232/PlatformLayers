/* PlatformLayers/GameLibs/file_io.h */
#ifndef FILE_IO_SENTRY
#define FILE_IO_SENTRY
// @TODO: include path
#include "../os.h"

#include <string.h>

typedef struct mapped_file_crawler_tag {
    mapped_file_t *file;
    const u8 *pos;
} mapped_file_crawler_t;

mapped_file_crawler_t make_crawler(mapped_file_t *file);

inline u64 crawler_bytes_read(mapped_file_crawler_t *crawler)
{
    ASSERT(crawler->pos >= crawler->file->mem);
    return crawler->pos - crawler->file->mem;
}

inline u64 crawler_bytes_left(mapped_file_crawler_t *crawler)
{
    ASSERT(crawler->file->byte_size >= crawler_bytes_read(crawler));
    return crawler->file->byte_size - crawler_bytes_read(crawler);
}

bool consume_file_chunk(mapped_file_crawler_t *crawler,
                        void *out_mem, u32 chunk_size);

#define CONSUME(_crawler, _out_struct) consume_file_chunk(_crawler, _out_struct, sizeof(*_out_struct))
#define DISCARD(_crawler, _size) consume_file_chunk(_crawler, NULL, _size)

#endif
