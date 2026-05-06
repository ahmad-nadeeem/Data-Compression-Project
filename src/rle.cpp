#include "bzip2.h"

void rle1_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    if (len == 0) {
        *out_len = 0;
        return;
    }
    size_t out_idx = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char current_char = input[i];
        unsigned char count = 1;
        while (i + 1 < len && input[i + 1] == current_char && count < 255) {
            count++;
            i++;
        }
        output[out_idx++] = current_char;
        output[out_idx++] = count;
    }
    *out_len = out_idx;
}

void rle1_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    size_t out_idx = 0;
    for (size_t i = 0; i < len; i += 2) {
        unsigned char current_char = input[i];
        unsigned char count = input[i + 1];
        for (int j = 0; j < count; ++j) {
            output[out_idx++] = current_char;
        }
    }
    *out_len = out_idx;
}


void rle2_encode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    size_t out_idx = 0;
    
    for(size_t i = 0; i < len; i++) {
        if (input[i] == 0) {
            // Count consecutive zeros
            unsigned char count = 0;
            while (i < len && input[i] == 0 && count < 255) {
                count++;
                i++;
            }
            i--; // Adjust because the outer loop will increment i again
            
            output[out_idx++] = 0;     // Marker for zero-run
            output[out_idx++] = count; // How many zeros
        } else {
            // Leave non-zeros exactly as they are
            output[out_idx++] = input[i];
        }
    }
    *out_len = out_idx;
}

void rle2_decode(unsigned char *input, size_t len, unsigned char *output, size_t *out_len) {
    size_t out_idx = 0;
    
    for(size_t i = 0; i < len; i++) {
        if (input[i] == 0) {
            i++; // Move to the count byte
            unsigned char count = input[i];
            for(int j = 0; j < count; j++) {
                output[out_idx++] = 0;
            }
        } else {
            output[out_idx++] = input[i];
        }
    }
    *out_len = out_idx;
}
