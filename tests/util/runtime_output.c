#include "runtime_output.h"
#include "process.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

/** Подсчитывает ожидаемые символы всех итераций, независимо от порядка исполнения. */
static void expected_characters(int children, size_t counts[UCHAR_MAX + 1]) {
    for (int id = 1; id <= children; ++id) {
        for (int iteration = 1; iteration <= 5 * id; ++iteration) {
            char line[128];
            snprintf(line, sizeof(line), "process %d is doing %d iteration out of %d\n",
                     id, iteration, 5 * id);
            for (const unsigned char* cursor = (const unsigned char*)line; *cursor; ++cursor) {
                ++counts[*cursor];
            }
        }
    }
}

/** Проверяет строки неперемешанного вывода и последовательность итераций каждого ребёнка. */
static int valid_ordered_lines(const char* output, int children) {
    int iterations[PROCESS_LIMIT] = {0};
    const char* cursor = output;
    while (*cursor != '\0') {
        int id, iteration, total, length = 0;
        int fields = sscanf(cursor, "process %d is doing %d iteration out of %d%n",
                            &id, &iteration, &total, &length);
        if (fields != 3 || cursor[length] != '\n' || id < 1 || id > children) {
            return 0;
        }
        if (iteration != iterations[id] + 1 || total != 5 * id) {
            return 0;
        }
        ++iterations[id];
        cursor += length + 1;
    }

    for (int id = 1; id <= children; ++id) {
        if (iterations[id] != 5 * id) {
            return 0;
        }
    }
    return 1;
}

int test_runtime_output_valid(const char* output, int children, int mutex_enabled) {
    if (children < 0 || children > MAX_PROCESS_ID) {
        return 0;
    }

    size_t expected[UCHAR_MAX + 1] = {0};
    size_t actual[UCHAR_MAX + 1] = {0};
    expected_characters(children, expected);
    for (const unsigned char* cursor = (const unsigned char*)output; *cursor; ++cursor) {
        ++actual[*cursor];
    }
    if (memcmp(expected, actual, sizeof(expected)) != 0) {
        return 0;
    }
    return !mutex_enabled || valid_ordered_lines(output, children);
}
