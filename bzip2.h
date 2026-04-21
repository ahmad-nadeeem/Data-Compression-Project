#ifndef BZIP2_H
#define BZIP2_H

#include <cstddef>
#include <string>
#include <unordered_map>

// --- Data Structures ---
typedef struct {
    unsigned char *data;
    size_t size;
    size_t original_size;
} Block;

typedef struct {
    Block *blocks;
    int num_blocks;
    size_t block_size;
} BlockManager;

typedef struct {
    int index;
    char *rotation;
} Rotation;

// --- Function Prototypes ---
// Configuration
std::unordered_map<std::string, std::string> load_config(const std::string& filename);

// Block Management
BlockManager divide_into_blocks(const char *filename, size_t block_size);
int reassemble_blocks(BlockManager *manager, const char *output_filename);
void free_block_manager(BlockManager *manager);

// RLE-1
void rle1_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len);
void rle1_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len);

// BWT
void bwt_encode(unsigned char *input, size_t len, unsigned char *output, int *primary_index);
void bwt_decode(unsigned char *input, size_t len, int primary_index, unsigned char *output);

#endif
