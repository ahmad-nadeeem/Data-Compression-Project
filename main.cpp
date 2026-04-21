#include "bzip2.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <iomanip>

// --- Debug Helper Function ---
void print_buffer(const std::string& label, unsigned char* data, size_t len, int primary_index = -1) {
    std::cout << "\n=== " << label << " (Size: " << len << ") ===\n";
    
    if (primary_index != -1) {
        std::cout << "Primary Index: " << primary_index << "\n";
    }

    std::cout << "Data [Char|Hex]: ";
    for (size_t i = 0; i < len; ++i) {
        // If it's a printable ASCII character, show it. Otherwise, show a dot '.'
        if (data[i] >= 32 && data[i] <= 126) {
            std::cout << "[" << data[i] << "|";
        } else {
            std::cout << "[.|";
        }
        // Print the hex value
        printf("%02X] ", data[i]);
    }
    std::cout << "\n=========================================\n";
}

// --- Block Management Functions ---
BlockManager divide_into_blocks(const char *filename, size_t block_size) {
    BlockManager manager;
    manager.block_size = block_size;
    manager.blocks = nullptr;
    manager.num_blocks = 0;
    FILE* file = fopen(filename, "rb");
    if (!file) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return manager;
    }
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
        fread(manager.blocks[i].data, 1, current_block_size, file);
    }
    fclose(file);
    return manager;
}

void free_block_manager(BlockManager *manager) {
    if (manager && manager->blocks) {
        for (int i = 0; i < manager->num_blocks; ++i) { free(manager->blocks[i].data); }
        free(manager->blocks);
        manager->blocks = nullptr;
        manager->num_blocks = 0;
    }
}

int main() {
    std::cout << "Starting BZip2 Stage 1 Trace..." << std::endl;

    auto config = load_config("config.ini");
    size_t block_size = config.count("block_size") ? std::stoull(config["block_size"]) : 500000;

    const char* input_file = "test.txt";
    BlockManager manager = divide_into_blocks(input_file, block_size);
    
    if (manager.num_blocks == 0) return 1;

    Block& block = manager.blocks[0];
    
    // Print Original
    print_buffer("1. ORIGINAL INPUT", block.data, block.size);

    // --- ENCODING ---
    std::vector<unsigned char> rle_encoded(block.size * 2);
    size_t rle_len = 0;
    rle1_encode(block.data, block.size, rle_encoded.data(), &rle_len);
    
    // Print RLE-1 Output
    print_buffer("2. AFTER RLE-1 ENCODE", rle_encoded.data(), rle_len);

    std::vector<unsigned char> bwt_encoded(rle_len);
    int primary_index = 0;
    bwt_encode(rle_encoded.data(), rle_len, bwt_encoded.data(), &primary_index);

    // Print BWT Output
    print_buffer("3. AFTER BWT ENCODE", bwt_encoded.data(), rle_len, primary_index);

    // --- DECODING ---
    std::vector<unsigned char> bwt_decoded(rle_len);
    bwt_decode(bwt_encoded.data(), rle_len, primary_index, bwt_decoded.data());

    // Print BWT Decode Output (Should match RLE-1 Encode exactly)
    print_buffer("4. AFTER BWT DECODE", bwt_decoded.data(), rle_len);

    std::vector<unsigned char> final_decoded(block.original_size);
    size_t final_len = 0;
    rle1_decode(bwt_decoded.data(), rle_len, final_decoded.data(), &final_len);

    // Print Final Output
    print_buffer("5. AFTER RLE-1 DECODE (FINAL)", final_decoded.data(), final_len);

    if (final_len == block.original_size && memcmp(block.data, final_decoded.data(), final_len) == 0) {
        std::cout << "\nSUCCESS! Decoded data matches original block perfectly.\n" << std::endl;
    } else {
        std::cout << "\nFAILED! Mismatch detected.\n" << std::endl;
    }

    free_block_manager(&manager);
    return 0;
}
