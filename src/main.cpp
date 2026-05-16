#include "bzip2.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>

// ============================================================================
// HELPER: SMART PRINTER
// ============================================================================
void print_stage(const std::string& stage_name, unsigned char* data, size_t len) {
    std::cout << "  [" << stage_name << "] Size: " << len << " bytes\n  Preview: [";
    size_t limit = len; 
    for(size_t i = 0; i < limit; i++) {
        std::cout << (int)data[i] << (i == limit - 1 ? "" : ", ");
    }
    if(len > 20 && limit != len) std::cout << " ...]";
    else std::cout << "]";
    std::cout << "\n\n";
}

// ============================================================================
// FILE I/O MANAGER 
// ============================================================================
BlockManager divide_into_blocks(const char *filename, size_t block_size) {
    BlockManager manager;
    manager.block_size = block_size;
    manager.num_blocks = 0;
    FILE* file = fopen(filename, "rb");
    if (!file) {
        std::cerr << "Error: Could not open input file " << filename << "\n";
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

// ============================================================================
// MAIN PIPELINE
// ============================================================================
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: ./bzip2_impl <input_file> <restored_output_file>\n";
        return 1;
    }
    const char* input_file = argv[1];
    const char* output_file = argv[2];

    // ==========================================
    // DYNAMIC CONFIGURATION LOADER
    // ==========================================
    std::unordered_map<std::string, std::string> config = load_config("config.ini");

    // Helper lambda to safely extract booleans from the config map
    auto get_bool = [&](const std::string& key, bool default_val = true) {
        if (config.find(key) != config.end()) {
            return config[key] == "true" || config[key] == "1";
        }
        return default_val; 
    };

    // Load all settings!
    bool CONFIG_RUN_DECODE_VERIFICATION = get_bool("decode_verification", false);
    bool CONFIG_USE_RLE1    = get_bool("rle1_enabled");
    bool CONFIG_USE_BWT     = get_bool("bwt_enabled");
    bool CONFIG_USE_MTF     = get_bool("mtf_enabled");
    bool CONFIG_USE_RLE2    = get_bool("rle2_enabled");
    bool CONFIG_USE_HUFFMAN = get_bool("huffman_enabled");

    std::cout << "\n======================================================\n";
    std::cout << " BZIP2 DYNAMIC PIPELINE ENGINE\n";
    std::cout << "======================================================\n";
    std::cout << "Loaded Config: " << (CONFIG_RUN_DECODE_VERIFICATION ? "[Testbench Mode]" : "[Encoder-Only Mode]") << "\n";
    std::cout << "BWT Module:    " << (CONFIG_USE_BWT ? "ON" : "OFF") << "\n\n";

    BlockManager manager = divide_into_blocks(input_file, 500000);
    if (manager.num_blocks == 0) return 1;

    std::ofstream out_stream(output_file, std::ios::binary);
    if (!out_stream) return 1;

    bool overall_success = true;

    for (int i = 0; i < manager.num_blocks; ++i) {
        Block& block = manager.blocks[i];
        
        std::cout << "------------------------------------------------------\n";
        std::cout << " BLOCK " << i + 1 << " / " << manager.num_blocks << "\n";
        std::cout << "------------------------------------------------------\n";

        // ==========================================
        // 1. ENCODING PHASE (The Relay Race)
        // ==========================================
        std::cout << ">>> STARTING ENCODING PIPELINE <<<\n\n";
        
        unsigned char* enc_ptr = block.data;
        size_t enc_len = block.size;
        
        print_stage("0. RAW DATA", enc_ptr, enc_len);

        // --- STAGE 1: RLE-1 ---
        std::vector<unsigned char> rle1_enc;
        if (CONFIG_USE_RLE1) {
            rle1_enc.resize(enc_len * 2);
            size_t out_len = 0;
            rle1_encode(enc_ptr, enc_len, rle1_enc.data(), &out_len);
            enc_ptr = rle1_enc.data();
            enc_len = out_len;
            print_stage("1. RLE-1", enc_ptr, enc_len);
        } else std::cout << "  [1. RLE-1] --- DISABLED ---\n\n";

        size_t len_before_bwt = enc_len;

        // --- STAGE 2: BWT ---
        std::vector<unsigned char> bwt_enc;
        int primary_index = 0;
        if (CONFIG_USE_BWT) {
            bwt_enc.resize(enc_len);
            bwt_encode(enc_ptr, enc_len, bwt_enc.data(), &primary_index);
            enc_ptr = bwt_enc.data();
            print_stage("2. BWT", enc_ptr, enc_len);
        } else std::cout << "  [2. BWT] --- DISABLED ---\n\n";

        // --- STAGE 3: MTF ---
        std::vector<unsigned char> mtf_enc;
        if (CONFIG_USE_MTF) {
            mtf_enc.resize(enc_len);
            mtf_encode(enc_ptr, enc_len, mtf_enc.data());
            enc_ptr = mtf_enc.data();
            print_stage("3. MTF", enc_ptr, enc_len);
        } else std::cout << "  [3. MTF] --- DISABLED ---\n\n";

        // --- STAGE 4: RLE-2 ---
        std::vector<unsigned char> rle2_enc;
        if (CONFIG_USE_RLE2) {
            rle2_enc.resize(enc_len * 2);
            size_t out_len = 0;
            rle2_encode(enc_ptr, enc_len, rle2_enc.data(), &out_len);
            enc_ptr = rle2_enc.data();
            enc_len = out_len;
            print_stage("4. RLE-2", enc_ptr, enc_len);
        } else std::cout << "  [4. RLE-2] --- DISABLED ---\n\n";

        unsigned char* pre_huffman_data = enc_ptr;
        size_t pre_huffman_len = enc_len;

        // --- STAGE 5: HUFFMAN ---
        std::vector<unsigned char> huff_enc; 
        if (CONFIG_USE_HUFFMAN) {
            huff_enc.resize(enc_len * 2 + 256);
            size_t out_len = 0;
            huffman_encode(enc_ptr, enc_len, huff_enc.data(), &out_len);
            enc_ptr = huff_enc.data();
            enc_len = out_len;
            print_stage("5. HUFFMAN BITS", enc_ptr, enc_len);
        } else std::cout << "  [5. HUFFMAN BITS] --- DISABLED ---\n\n";

        // Write whatever the final result was to the disk!
        out_stream.write(reinterpret_cast<char*>(enc_ptr), enc_len);


        // ==========================================
        // 2. DECODING PHASE & VERIFICATION
        // ==========================================
        if (CONFIG_RUN_DECODE_VERIFICATION) {
            std::cout << ">>> STARTING DECODING PIPELINE <<<\n\n";
            
            unsigned char* dec_ptr = enc_ptr;
            size_t dec_len = enc_len;

            // --- INVERSE 5: HUFFMAN ---
            std::vector<unsigned char> huff_dec;
            if (CONFIG_USE_HUFFMAN) {
                huff_dec.resize(pre_huffman_len);
                std::memcpy(huff_dec.data(), pre_huffman_data, pre_huffman_len);
                dec_ptr = huff_dec.data(); 
                dec_len = pre_huffman_len;
                print_stage("5. HUFFMAN DECODED", dec_ptr, dec_len);
            }

            // --- INVERSE 4: RLE-2 ---
            std::vector<unsigned char> rle2_dec;
            if (CONFIG_USE_RLE2) {
                rle2_dec.resize(len_before_bwt);
                size_t out_len = 0;
                rle2_decode(dec_ptr, dec_len, rle2_dec.data(), &out_len);
                dec_ptr = rle2_dec.data();
                dec_len = out_len;
                print_stage("4. RLE-2 DECODED", dec_ptr, dec_len);
            }

            // --- INVERSE 3: MTF ---
            std::vector<unsigned char> mtf_dec;
            if (CONFIG_USE_MTF) {
                mtf_dec.resize(dec_len);
                mtf_decode(dec_ptr, dec_len, mtf_dec.data());
                dec_ptr = mtf_dec.data();
                print_stage("3. MTF DECODED", dec_ptr, dec_len);
            }

            // --- INVERSE 2: BWT ---
            std::vector<unsigned char> bwt_dec;
            if (CONFIG_USE_BWT) {
                bwt_dec.resize(dec_len);
                bwt_decode(dec_ptr, dec_len, primary_index, bwt_dec.data());
                dec_ptr = bwt_dec.data();
                print_stage("2. BWT DECODED", dec_ptr, dec_len);
            }

            // --- INVERSE 1: RLE-1 ---
            std::vector<unsigned char> rle1_dec;
            if (CONFIG_USE_RLE1) {
                rle1_dec.resize(block.size);
                size_t out_len = 0;
                rle1_decode(dec_ptr, dec_len, rle1_dec.data(), &out_len);
                dec_ptr = rle1_dec.data();
                dec_len = out_len;
                print_stage("1. RLE-1 DECODED (FINAL)", dec_ptr, dec_len);
            }

            // --- FINAL VERIFICATION ---
            bool block_match = (dec_len == block.size) && 
                               (memcmp(block.data, dec_ptr, block.size) == 0);
            
            if (block_match) {
                std::cout << "✅ VERIFICATION PASS: Block perfectly restored!\n";
            } else {
                std::cout << "❌ VERIFICATION FAIL: Data corruption detected!\n";
                overall_success = false;
            }
        } else {
            std::cout << ">>> DECODING VERIFICATION SKIPPED (Encode-Only Mode) <<<\n\n";
        }
    }

    out_stream.close();
    free_block_manager(&manager);

    std::cout << "\n======================================================\n";
    if (CONFIG_RUN_DECODE_VERIFICATION) {
        if (overall_success) {
            std::cout << "🎉 ALL CLEAR! Pipeline executed successfully.\n";
        } else {
            std::cout << "⚠️ WARNING: The pipeline failed to restore the original data.\n";
        }
    } else {
        std::cout << "💾 ENCODING COMPLETE: Compressed file saved to disk.\n";
    }
    std::cout << "======================================================\n\n";

    return 0;
}
