#include "bzip2.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <chrono>   
#include <iomanip>  

// ============================================================================
// HELPER: SMART PRINTER
// ============================================================================
void print_stage(const std::string& stage_name, unsigned char* data, size_t len) {
    std::cout << "  [" << stage_name << "] Size: " << len << " bytes\n  Preview: [";
    size_t limit = len > 20 ? 20 : len; 
    for(size_t i = 0; i < limit; i++) {
        std::cout << (int)data[i] << (i == limit - 1 ? "" : ", ");
    }
    if(len > 20) std::cout << " ...]";
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
        std::cerr << "Usage: ./bzip2_impl <input_file> <output_file>\n";
        return 1;
    }
    const char* input_file = argv[1];
    const char* output_file = argv[2];

    std::unordered_map<std::string, std::string> config = load_config("config.ini");
    auto get_bool = [&](const std::string& key, bool default_val = true) {
        if (config.find(key) != config.end()) {
            std::string val = config[key];
            return val.find("true") != std::string::npos || val == "1";
        }
        return default_val; 
    };

    bool CONFIG_RUN_ENCODE              = get_bool("encode_mode", true); 
    bool CONFIG_RUN_DECODE_VERIFICATION = get_bool("decode_verification", false);
    
    bool CONFIG_USE_RLE1    = get_bool("rle1_enabled");
    bool CONFIG_USE_BWT     = get_bool("bwt_enabled");
    bool CONFIG_USE_MTF     = get_bool("mtf_enabled");
    bool CONFIG_USE_RLE2    = get_bool("rle2_enabled");
    bool CONFIG_USE_HUFFMAN = get_bool("huffman_enabled");

    std::cout << "\n======================================================\n";
    std::cout << " BZIP2 DZIP ENGINE (METRICS & DECODER COMPLETE)\n";
    std::cout << "======================================================\n";
    std::cout << "Operation Mode: " << (CONFIG_RUN_ENCODE ? "[ENCODER]" : "[STANDALONE DECOMPRESSOR]") << "\n\n";

    double total_rle1_time = 0, total_bwt_time = 0, total_mtf_time = 0, total_rle2_time = 0, total_huff_time = 0;
    double total_rle1_dec_time = 0, total_bwt_dec_time = 0, total_mtf_dec_time = 0, total_rle2_dec_time = 0, total_huff_dec_time = 0;
    size_t total_input_bytes = 0;
    size_t total_output_bytes = 0;

    auto pipeline_start_clock = std::chrono::high_resolution_clock::now();

    // ------------------------------------------------------------------------
    // MODE 1: STANDALONE DECOMPRESSOR MODE
    // ------------------------------------------------------------------------
    if (!CONFIG_RUN_ENCODE) {
        std::ifstream compressed_stream(input_file, std::ios::binary);
        if (!compressed_stream) {
            std::cerr << "Error: Could not open compressed file " << input_file << "\n";
            return 1;
        }

        std::ofstream decoded_stream(output_file, std::ios::binary);
        if (!decoded_stream) return 1;

        std::cout << ">>> STARTING STANDALONE DECODING PIPELINE <<<\n\n";

        size_t compressed_size = 0, original_block_size = 0, len_before_bwt = 0, pre_huffman_len = 0;
        int primary_index = 0;

        // Read the full 5-part binary tracking header sequentially
        compressed_stream.read(reinterpret_cast<char*>(&compressed_size), sizeof(compressed_size));
        compressed_stream.read(reinterpret_cast<char*>(&original_block_size), sizeof(original_block_size));
        compressed_stream.read(reinterpret_cast<char*>(&len_before_bwt), sizeof(len_before_bwt));
        compressed_stream.read(reinterpret_cast<char*>(&pre_huffman_len), sizeof(pre_huffman_len));
        compressed_stream.read(reinterpret_cast<char*>(&primary_index), sizeof(primary_index));

        std::vector<unsigned char> packed_data(compressed_size);
        compressed_stream.read(reinterpret_cast<char*>(packed_data.data()), compressed_size);

        unsigned char* dec_ptr = packed_data.data();
        size_t dec_len = compressed_size;

        print_stage("0. READ PACKED BYTES FROM DISK", dec_ptr, dec_len);

        // --- INVERSE 5: HUFFMAN ---
        std::vector<unsigned char> huff_dec;
        if (CONFIG_USE_HUFFMAN) {
            huff_dec.resize(pre_huffman_len * 2 + 1000); 
            size_t out_len = 0;

            auto t_start = std::chrono::high_resolution_clock::now();
            huffman_decode(dec_ptr, dec_len, huff_dec.data(), &out_len);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_huff_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            dec_ptr = huff_dec.data(); 
            dec_len = pre_huffman_len; // Drop bit padding junk values explicitly
            print_stage("5. HUFFMAN DECODED", dec_ptr, dec_len);
        }

        // --- INVERSE 4: RLE-2 ---
        std::vector<unsigned char> rle2_dec;
        if (CONFIG_USE_RLE2) {
            rle2_dec.resize(len_before_bwt * 2 + 1000); 
            size_t out_len = 0;

            auto t_start = std::chrono::high_resolution_clock::now();
            rle2_decode(dec_ptr, dec_len, rle2_dec.data(), &out_len);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_rle2_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            dec_ptr = rle2_dec.data();
            dec_len = out_len;
            print_stage("4. RLE-2 DECODED", dec_ptr, dec_len);
        }

        // --- INVERSE 3: MTF ---
        std::vector<unsigned char> mtf_dec;
        if (CONFIG_USE_MTF) {
            mtf_dec.resize(dec_len);

            auto t_start = std::chrono::high_resolution_clock::now();
            mtf_decode(dec_ptr, dec_len, mtf_dec.data());
            auto t_end = std::chrono::high_resolution_clock::now();
            total_mtf_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            dec_ptr = mtf_dec.data();
            print_stage("3. MTF DECODED", dec_ptr, dec_len);
        }

        // --- INVERSE 2: BWT ---
        std::vector<unsigned char> bwt_dec;
        if (CONFIG_USE_BWT) {
            bwt_dec.resize(dec_len);

            auto t_start = std::chrono::high_resolution_clock::now();
            bwt_decode(dec_ptr, dec_len, primary_index, bwt_dec.data());
            auto t_end = std::chrono::high_resolution_clock::now();
            total_bwt_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            dec_ptr = bwt_dec.data();
            print_stage("2. BWT DECODED", dec_ptr, dec_len);
        }

        // --- INVERSE 1: RLE-1 ---
        std::vector<unsigned char> rle1_dec;
        if (CONFIG_USE_RLE1) {
            rle1_dec.resize(original_block_size * 10 + 100000);
            size_t out_len = 0;

            auto t_start = std::chrono::high_resolution_clock::now();
            rle1_decode(dec_ptr, dec_len, rle1_dec.data(), &out_len);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_rle1_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            dec_ptr = rle1_dec.data();
            dec_len = out_len;
            print_stage("1. RLE-1 DECODED (FINAL)", dec_ptr, dec_len);
        }

        decoded_stream.write(reinterpret_cast<char*>(dec_ptr), dec_len);
        
        auto pipeline_end_clock = std::chrono::high_resolution_clock::now();
        double global_runtime = std::chrono::duration<double, std::milli>(pipeline_end_clock - pipeline_start_clock).count();

        std::cout << "======================================================\n";
        std::cout << "              DECOMPRESSION PERFORMANCE METRICS        \n";
        std::cout << "======================================================\n";
        std::cout << "  Restored Output Size: " << dec_len << " bytes\n";
        std::cout << "  Total Decode Latency: " << global_runtime << " ms\n";
        std::cout << "------------------------------------------------------\n";
        std::cout << "  1. Huffman Decoder:   " << total_huff_dec_time << " ms\n";
        std::cout << "  2. RLE-2 Module:       " << total_rle2_dec_time << " ms\n";
        std::cout << "  3. MTF Module:         " << total_mtf_dec_time << " ms\n";
        std::cout << "  4. BWT Module:         " << total_bwt_dec_time << " ms\n";
        std::cout << "  5. RLE-1 Module:       " << total_rle1_dec_time << " ms\n";
        std::cout << "======================================================\n\n";

        compressed_stream.close();
        decoded_stream.close();
        return 0;
    }

    // ------------------------------------------------------------------------
    // MODE 2: ENCODER MODE
    // ------------------------------------------------------------------------
    BlockManager manager = divide_into_blocks(input_file, 500000);
    if (manager.num_blocks == 0) return 1;

    std::ofstream out_stream(output_file, std::ios::binary);
    if (!out_stream) return 1;

    bool overall_success = true;

    for (int i = 0; i < manager.num_blocks; ++i) {
        Block& block = manager.blocks[i];
        total_input_bytes += block.size;
        
        std::cout << "------------------------------------------------------\n";
        std::cout << " BLOCK " << i + 1 << " / " << manager.num_blocks << " (" << block.size << " bytes)\n";
        std::cout << "------------------------------------------------------\n";

        std::cout << ">>> STARTING ENCODING PIPELINE <<<\n\n";
        unsigned char* enc_ptr = block.data;
        size_t enc_len = block.size;
        size_t original_block_size = block.size;
        
        print_stage("0. RAW DATA", enc_ptr, enc_len);

        // --- STAGE 1: RLE-1 ---
        std::vector<unsigned char> rle1_enc;
        if (CONFIG_USE_RLE1) {
            rle1_enc.resize(enc_len * 2);
            size_t out_len = 0;

            auto t_start = std::chrono::high_resolution_clock::now();
            rle1_encode(enc_ptr, enc_len, rle1_enc.data(), &out_len);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_rle1_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            enc_ptr = rle1_enc.data();
            enc_len = out_len;
            print_stage("1. RLE-1", enc_ptr, enc_len);
        }

        size_t len_before_bwt = enc_len;

        // --- STAGE 2: BWT ---
        std::vector<unsigned char> bwt_enc;
        int primary_index = 0;
        if (CONFIG_USE_BWT) {
            bwt_enc.resize(enc_len);

            auto t_start = std::chrono::high_resolution_clock::now();
            bwt_encode(enc_ptr, enc_len, bwt_enc.data(), &primary_index);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_bwt_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            enc_ptr = bwt_enc.data();
            print_stage("2. BWT", enc_ptr, enc_len);
        }

        // --- STAGE 3: MTF ---
        std::vector<unsigned char> mtf_enc;
        if (CONFIG_USE_MTF) {
            mtf_enc.resize(enc_len);

            auto t_start = std::chrono::high_resolution_clock::now();
            mtf_encode(enc_ptr, enc_len, mtf_enc.data());
            auto t_end = std::chrono::high_resolution_clock::now();
            total_mtf_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            enc_ptr = mtf_enc.data();
            print_stage("3. MTF", enc_ptr, enc_len);
        }

        // --- STAGE 4: RLE-2 ---
        std::vector<unsigned char> rle2_enc;
        if (CONFIG_USE_RLE2) {
            rle2_enc.resize(enc_len * 2);
            size_t out_len = 0;

            auto t_start = std::chrono::high_resolution_clock::now();
            rle2_encode(enc_ptr, enc_len, rle2_enc.data(), &out_len);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_rle2_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            enc_ptr = rle2_enc.data();
            enc_len = out_len;
            print_stage("4. RLE-2", enc_ptr, enc_len);
        }

        size_t pre_huffman_len = enc_len;

        // --- STAGE 5: HUFFMAN ---
        std::vector<unsigned char> huff_enc; 
        if (CONFIG_USE_HUFFMAN) {
            huff_enc.resize(enc_len * 2 + 256);
            size_t out_len = 0;

            auto t_start = std::chrono::high_resolution_clock::now();
            huffman_encode(enc_ptr, enc_len, huff_enc.data(), &out_len);
            auto t_end = std::chrono::high_resolution_clock::now();
            total_huff_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();

            enc_ptr = huff_enc.data();
            enc_len = out_len;
            print_stage("5. HUFFMAN BITS", enc_ptr, enc_len);
        }

        total_output_bytes += enc_len;

        // Write complete structural 5-part tracking header layout to disk securely
        out_stream.write(reinterpret_cast<char*>(&enc_len), sizeof(enc_len));
        out_stream.write(reinterpret_cast<char*>(&original_block_size), sizeof(original_block_size));
        out_stream.write(reinterpret_cast<char*>(&len_before_bwt), sizeof(len_before_bwt));
        out_stream.write(reinterpret_cast<char*>(&pre_huffman_len), sizeof(pre_huffman_len));
        out_stream.write(reinterpret_cast<char*>(&primary_index), sizeof(primary_index));
        out_stream.write(reinterpret_cast<char*>(enc_ptr), enc_len);

        // --- IN-MEMORY VERIFICATION CHECK ---
        if (CONFIG_RUN_DECODE_VERIFICATION) {
            std::cout << ">>> STARTING DECODING PIPELINE <<<\n\n";
            unsigned char* dec_ptr = enc_ptr;
            size_t dec_len = enc_len;

            std::vector<unsigned char> huff_dec;
            if (CONFIG_USE_HUFFMAN) {
                auto t_start = std::chrono::high_resolution_clock::now();
                huff_dec.resize(pre_huffman_len * 2 + 1000);
                size_t out_len = 0;
                huffman_decode(dec_ptr, dec_len, huff_dec.data(), &out_len);
                dec_ptr = huff_dec.data(); 
                dec_len = pre_huffman_len; // FIX: Overwrite bounds with pre_huffman_len to drop noise bits!
                auto t_end = std::chrono::high_resolution_clock::now();
                total_huff_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();
                print_stage("5. HUFFMAN DECODED", dec_ptr, dec_len);
            }

            std::vector<unsigned char> rle2_dec;
            if (CONFIG_USE_RLE2) {
                rle2_dec.resize(len_before_bwt * 2 + 1000);
                size_t out_len = 0;
                auto t_start = std::chrono::high_resolution_clock::now();
                rle2_decode(dec_ptr, dec_len, rle2_dec.data(), &out_len);
                auto t_end = std::chrono::high_resolution_clock::now();
                total_rle2_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();
                dec_ptr = rle2_dec.data();
                dec_len = out_len;
                print_stage("4. RLE-2 DECODED", dec_ptr, dec_len);
            }

            std::vector<unsigned char> mtf_dec;
            if (CONFIG_USE_MTF) {
                mtf_dec.resize(dec_len);
                auto t_start = std::chrono::high_resolution_clock::now();
                mtf_decode(dec_ptr, dec_len, mtf_dec.data());
                auto t_end = std::chrono::high_resolution_clock::now();
                total_mtf_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();
                dec_ptr = mtf_dec.data();
                print_stage("3. MTF DECODED", dec_ptr, dec_len);
            }

            std::vector<unsigned char> bwt_dec;
            if (CONFIG_USE_BWT) {
                bwt_dec.resize(dec_len);
                auto t_start = std::chrono::high_resolution_clock::now();
                bwt_decode(dec_ptr, dec_len, primary_index, bwt_dec.data());
                auto t_end = std::chrono::high_resolution_clock::now();
                total_bwt_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();
                dec_ptr = bwt_dec.data();
                print_stage("2. BWT DECODED", dec_ptr, dec_len);
            }

            std::vector<unsigned char> rle1_dec;
            if (CONFIG_USE_RLE1) {
                rle1_dec.resize(block.size * 10 + 100000);
                size_t out_len = 0;
                auto t_start = std::chrono::high_resolution_clock::now();
                rle1_decode(dec_ptr, dec_len, rle1_dec.data(), &out_len);
                auto t_end = std::chrono::high_resolution_clock::now();
                total_rle1_dec_time += std::chrono::duration<double, std::milli>(t_end - t_start).count();
                dec_ptr = rle1_dec.data();
                dec_len = out_len;
                print_stage("1. RLE-1 DECODED (FINAL)", dec_ptr, dec_len);
            }

            bool block_match = (dec_len == block.size) && (memcmp(block.data, dec_ptr, block.size) == 0);
            if (block_match) std::cout << "✅ VERIFICATION PASS: Block perfectly restored!\n";
            else { std::cout << "❌ VERIFICATION FAIL: Data corruption!\n"; overall_success = false; }
        }
    }

    auto pipeline_end_clock = std::chrono::high_resolution_clock::now();
    double global_runtime = std::chrono::duration<double, std::milli>(pipeline_end_clock - pipeline_start_clock).count();

    out_stream.close();
    free_block_manager(&manager);

    double compression_ratio = (double)total_input_bytes / (total_output_bytes == 0 ? 1 : total_output_bytes);
    double space_savings = (1.0 - ((double)total_output_bytes / total_input_bytes)) * 100.0;

    std::cout << "\n======================================================\n";
    std::cout << "              PERFORMANCE ANALYSIS LOGS                \n";
    std::cout << "======================================================\n";
    std::cout << "  Input Dataset Size:  " << total_input_bytes << " bytes\n";
    std::cout << "  Output File Size:     " << total_output_bytes << " bytes\n";
    std::cout << "  Compression Ratio:    " << std::fixed << std::setprecision(3) << compression_ratio << "x\n";
    std::cout << "  Space Savings Factor: " << std::fixed << std::setprecision(2) << space_savings << " %\n";
    std::cout << "  Total Execution Time: " << global_runtime << " ms\n";
    std::cout << "------------------------------------------------------\n";
    std::cout << "            STAGE-BY-STAGE LATENCY PROFILER           \n";
    std::cout << "------------------------------------------------------\n";
    std::cout << "  [Stage Name]          [Encode Latency]     [Decode Latency]\n";
    std::cout << "  1. RLE-1 Module:       " << std::setw(10) << total_rle1_time << " ms        " << (CONFIG_RUN_DECODE_VERIFICATION ? std::to_string(total_rle1_dec_time) + " ms" : "N/A") << "\n";
    std::cout << "  2. BWT Module:         " << std::setw(10) << total_bwt_time << " ms        " << (CONFIG_RUN_DECODE_VERIFICATION ? std::to_string(total_bwt_dec_time) + " ms" : "N/A") << "\n";
    std::cout << "  3. MTF Module:         " << std::setw(10) << total_mtf_time << " ms        " << (CONFIG_RUN_DECODE_VERIFICATION ? std::to_string(total_mtf_dec_time) + " ms" : "N/A") << "\n";
    std::cout << "  4. RLE-2 Module:       " << std::setw(10) << total_rle2_time << " ms        " << (CONFIG_RUN_DECODE_VERIFICATION ? std::to_string(total_rle2_dec_time) + " ms" : "N/A") << "\n";
    std::cout << "  5. Huffman Encoder:    " << std::setw(10) << total_huff_time << " ms        " << (CONFIG_RUN_DECODE_VERIFICATION ? std::to_string(total_huff_dec_time) + " ms" : "N/A") << "\n";
    std::cout << "======================================================\n";
    if (CONFIG_RUN_DECODE_VERIFICATION) {
        if (overall_success) std::cout << "🎉 PIPELINE SUCCESS: Data symmetry mathematically validated.\n";
        else std::cout << "⚠️ CRITICAL FAILURE: Decoded streams do not match origin dataset.\n";
    } else std::cout << "💾 PIPELINE OUTPUT COMMITTED TO DISK SEAMLESSLY.\n";
    std::cout << "======================================================\n\n";

    return 0;
}
