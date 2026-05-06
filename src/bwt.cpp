#include "bzip2.h"
#include <vector>
#include <algorithm>
#include <cstdlib>
#include <cstring>

// --------------------------------------------------------
// EXTRA CREDIT: Suffix Array implementation
// --------------------------------------------------------
int *build_suffix_array(unsigned char *text, int n) {
    // Allocate an array to hold the integer indices
    int *sa = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) {
        sa[i] = i;
    }

    // Sort the indices based on the characters they point to in the text
    std::sort(sa, sa + n, [text, n](int a, int b) {
        // Compare the cyclic rotations starting at index a and b
        for (int i = 0; i < n; ++i) {
            unsigned char char_a = text[(a + i) % n];
            unsigned char char_b = text[(b + i) % n];
            if (char_a != char_b) {
                return char_a < char_b;
            }
        }
        return false; // They are perfectly equal
    });

    return sa;
}

// --------------------------------------------------------
// BWT Encode using the Memory-Efficient Suffix Array
// --------------------------------------------------------
void bwt_encode(unsigned char *input, size_t len, unsigned char *output, int *primary_index) {
    if (len == 0) return;

    // 1. Build the suffix array (Memory footprint: O(N) instead of O(N^2))
    int *sa = build_suffix_array(input, len);

    // 2. Extract the last column and find the primary index
    for (size_t i = 0; i < len; i++) {
        // The last character of the rotation starting at sa[i]
        // is the character immediately preceding it (with wrap-around)
        int last_char_index = (sa[i] + len - 1) % len;
        output[i] = input[last_char_index];

        // If this rotation starts at index 0, it's the original string
        if (sa[i] == 0) {
            *primary_index = i;
        }
    }

    free(sa); // Clean up our integer array
}

// --------------------------------------------------------
// BWT Decode (LF-Mapping) - Unchanged, already memory efficient
// --------------------------------------------------------
void bwt_decode(unsigned char *input, size_t len, int primary_index, unsigned char *output) {
    if (len == 0) return;

    int counts[256] = {0};
    for (size_t i = 0; i < len; i++) {
        counts[input[i]]++;
    }

    int offsets[256] = {0};
    int sum = 0;
    for (int i = 0; i < 256; i++) {
        offsets[i] = sum;
        sum += counts[i];
    }

    std::vector<int> next(len);
    int current_counts[256] = {0};
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = input[i];
        next[offsets[c] + current_counts[c]] = i;
        current_counts[c]++;
    }

    int current = next[primary_index];
    for (size_t i = 0; i < len; i++) {
        output[i] = input[current];
        current = next[current];
    }
}
