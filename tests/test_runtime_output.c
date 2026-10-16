#include "test_support.h"
#include "test_cases.h"
#include "runtime_output.h"

/** Строит полный вывод двух детей без смешивания строк. */
static void make_ordered_output(char* output, size_t capacity) {
    size_t used = 0;
    for (int id = 1; id <= 2; ++id) {
        for (int iteration = 1; iteration <= 5 * id; ++iteration) {
            int length = snprintf(output + used, capacity - used,
                                  "process %d is doing %d iteration out of %d\n",
                                  id, iteration, 5 * id);
            assert_true(length > 0 && (size_t)length < capacity - used);
            used += (size_t)length;
        }
    }
}

void test_runtime_output(void** state) {
    (void)state;
    char output[2048] = {0};
    make_ordered_output(output, sizeof(output));
    assert_true(test_runtime_output_valid(output, 2, 1));
    assert_true(test_runtime_output_valid(output, 2, 0));

    size_t length = strlen(output);
    for (size_t index = 0; index < length / 2; ++index) {
        char saved = output[index];
        output[index] = output[length - index - 1];
        output[length - index - 1] = saved;
    }
    assert_true(test_runtime_output_valid(output, 2, 0));
    assert_false(test_runtime_output_valid(output, 2, 1));

    output[0] = '!';
    assert_false(test_runtime_output_valid(output, 2, 0));
    assert_true(test_runtime_output_valid("", 0, 1));
}
