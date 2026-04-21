#include "bzip2.h"
#include <vector>
#include <cstring>
#include <cstdlib>

int compare_rotations(const void *a, const void *b) {
    Rotation *rotA = (Rotation *)a;
    Rotation *rotB = (Rotation *)b;
    return strcmp(rotA->rotation, rotB->rotation); 
}

void bwt_encode(unsigned char *input, size_t len, unsigned char *output, int *primary_index) {
    std::vector<Rotation> rotations(len);
    for (size_t i = 0; i < len; ++i) {
        rotations[i].index = i;
        rotations[i].rotation = (char*)malloc(len + 1);
        for (size_t j = 0; j < len; ++j) {
            rotations[i].rotation[j] = input[(i + j) % len];
        }
        rotations[i].rotation[len] = '\0';
    }
    qsort(rotations.data(), len, sizeof(Rotation), compare_rotations);
    for (size_t i = 0; i < len; ++i) {
        output[i] = rotations[i].rotation[len - 1];
        if (rotations[i].index == 0) {
            *primary_index = i;
        }
        free(rotations[i].rotation);
    }
}

void bwt_decode(unsigned char *input, size_t len, int primary_index, unsigned char *output) {
    std::vector<int> count(256, 0);
    std::vector<int> next(len);
    for (size_t i = 0; i < len; i++) { count[input[i]]++; }
    int sum = 0;
    for (int i = 0; i < 256; i++) {
        int temp = count[i];
        count[i] = sum;
        sum += temp;
    }
    for (size_t i = 0; i < len; i++) {
        next[count[input[i]]] = i;
        count[input[i]]++;
    }
    int current_index = next[primary_index];
    for (size_t i = 0; i < len; i++) {
        output[i] = input[current_index];
        current_index = next[current_index];
    }
}
