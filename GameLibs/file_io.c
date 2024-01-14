/* PlatformLayers/GameLibs/file_io.c */
#include "file_io.h"

mapped_file_crawler_t make_crawler(mapped_file_t *file)
{
    mapped_file_crawler_t crawler = { 0 };
    crawler.file = file;
    crawler.pos = file->mem;
    return crawler;
}

bool consume_file_chunk(mapped_file_crawler_t *crawler,
                        void *out_mem, u32 chunk_size)
{
    if (chunk_size == 0)
        return true;

    if (crawler_bytes_left(crawler) < chunk_size)
        return false;
    
    if (out_mem)
        memcpy(out_mem, crawler->pos, chunk_size);

    crawler->pos += chunk_size;
    return true;
}
