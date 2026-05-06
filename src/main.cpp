#include "bzip2.h"
#include <iostream>
#include <vector>
#include <cstring>

// Standard Linux library for measuring actual RAM usage
#ifdef __linux__
#include <sys/resource.h>
double get_memory_usage_mb() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss / 1024.0; // Convert KB to MB
}
#else
double get_memory_usage_mb() {
    return 0.0; // Fallback for Windows
}
#endif

// ... (Keep your divide_into_blocks and free_block_manager functions here) ...
BlockManager divide_into_blocks(const char *filename, size_t block_size) {
    BlockManager manager;
    manager.block_size = block_size;
    manager.num_blocks = 0;
    FILE* file = fopen(filename, "rb");
    if (!file) return manager;
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    manager.num_blocks = (file_size + block_size - 1) / block_size;
    manager.blocks = (Block*)malloc(manager.num_blocks * sizeof(Block));
    for (int i = 0; i < manager.num_blocks; ++i) {
        size_t current_block_size = (i == manager.num_blocks - 1 && file_size % block_size != 0) 
                                    ? (file_size % block_size) : block_size;
        manager.blocks[i].data = (unsigned char*)malloc(current_block_size);
        manager.blocks[i].size = current_block_size;
        manager.blocks[i].original_size = current_block_size;
        // Suppress the fread warning by capturing the return value
        size_t bytes_read = fread(manager.blocks[i].data, 1, current_block_size, file);
        (void)bytes_read; 
    }
    fclose(file);
    return manager;
}

void free_block_manager(BlockManager *manager) {
    if (manager && manager->blocks) {
        for (int i = 0; i < manager->num_blocks; ++i) free(manager->blocks[i].data);
        free(manager->blocks);
        manager->blocks = nullptr;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;
    const char* input_file = argv[1];

    BlockManager manager = divide_into_blocks(input_file, 500000);
    if (manager.num_blocks == 0) return 1;

    size_t total_original_size = 0;
    size_t total_compressed_size = 0;

    // Process every block in the file
    for (int i = 0; i < manager.num_blocks; ++i) {
        Block& block = manager.blocks[i];
        total_original_size += block.original_size;

        // Stage 1
        std::vector<unsigned char> rle1_enc(block.size * 2);
        size_t rle1_len = 0;
        rle1_encode(block.data, block.size, rle1_enc.data(), &rle1_len);
        
        std::vector<unsigned char> bwt_enc(rle1_len);
        int primary_index = 0;
        bwt_encode(rle1_enc.data(), rle1_len, bwt_enc.data(), &primary_index);
        
        // Stage 2
        std::vector<unsigned char> mtf_enc(rle1_len);
        mtf_encode(bwt_enc.data(), rle1_len, mtf_enc.data());
        
        std::vector<unsigned char> rle2_enc(rle1_len * 2);
        size_t rle2_len = 0;
        rle2_encode(mtf_enc.data(), rle1_len, rle2_enc.data(), &rle2_len);

        // Stage 3
        std::vector<unsigned char> huff_enc(rle2_len * 2 + 256); 
        size_t huff_len = 0;
        huffman_encode(rle2_enc.data(), rle2_len, huff_enc.data(), &huff_len);

        total_compressed_size += huff_len;
    }

    free_block_manager(&manager);

    // Print exactly what the Python script needs to read
    std::cout << "METRICS:" << total_original_size << "," << total_compressed_size << "," << get_memory_usage_mb() << std::endl;

    return 0;
}
