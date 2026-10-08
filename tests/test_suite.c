#include "test_support.h"
#include "test_cases.h"

#include <stdlib.h>
#include <unistd.h>

/** Подготавливает каналы и временный каталог перед каждым тестом. */
static int setup(void** state) {
    (void)state;
    test_ipc_setup();
    test_workspace_open();
    alarm(90);
    return 0;
}

/** Освобождает дескрипторы и временные файлы после каждого теста. */
static int teardown(void** state) {
    (void)state;
    alarm(0);
    test_ipc_teardown();
    test_workspace_close();
    return 0;
}

/** Проверяет функциональное поведение обычной сборки программы. */
static void test_application(void** state) {
    (void)state;
    const char* override = getenv("PA1_BINARY");
    test_program(override != NULL ? override : PA1_DEFAULT_BINARY);
}

/** Проверяет, что архив самодостаточен и соответствует формату сдачи. */
static void test_submission(void** state) {
    (void)state;
    test_package(PA1_REPOSITORY);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_ipc_wire_format, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_empty_and_maximum_payload, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_multicast, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_partial_reception, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_round_robin, setup, teardown),
        cmocka_unit_test_setup_teardown(test_application, setup, teardown),
        cmocka_unit_test_setup_teardown(test_submission, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
