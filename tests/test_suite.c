#include "test_support.h"
#include "test_cases.h"
#include "philosophers_model.h"

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
    test_model_close();
    test_workspace_close();
    return 0;
}

/** Проверяет функциональное поведение обычной сборки программы. */
static void test_application(void** state) {
    (void)state;
    const char* override = getenv("TEST_BINARY");
    test_program(override != NULL ? override : TEST_DEFAULT_BINARY);
}

/** Проверяет, что архив самодостаточен и соответствует формату сдачи. */
static void test_submission(void** state) {
    (void)state;
    test_package(TEST_REPOSITORY, 0);
}

/** Проверяет архив с оригинальной библиотекой, без подмены print() в приложении. */
static void test_linux_submission(void** state) {
    (void)state;
    alarm(300);
    test_package(TEST_REPOSITORY, 1);
}

/** Запускает выделенный сценарий проверки в целевой среде Linux x86-64. */
static int run_linux_test(void) {
#if defined(__linux__) && defined(__x86_64__)
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_linux_submission, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
#else
    (void)test_linux_submission;
    fputs("Original libruntime requires Linux x86-64. Run make test-linux there.\n", stderr);
    return EXIT_FAILURE;
#endif
}

/** Запускает обычные тесты либо проверку оригинальной Linux-библиотеки. */
int main(int argc, char** argv) {
    if (argc == 2 && strcmp(argv[1], "--linux") == 0) {
        return run_linux_test();
    }
    if (argc != 1) {
        fprintf(stderr, "Usage: %s [--linux]\n", argv[0]);
        return EXIT_FAILURE;
    }
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_ipc_wire_format, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_empty_and_maximum_payload, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_multicast, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_partial_reception, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_round_robin, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_backpressure, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_atomic_writes, setup, teardown),
        cmocka_unit_test_setup_teardown(test_ipc_errors, setup, teardown),
        cmocka_unit_test_setup_teardown(test_channels_nonblocking, setup, teardown),
        cmocka_unit_test_setup_teardown(test_philosophers_rules, setup, teardown),
        cmocka_unit_test_setup_teardown(test_philosophers_backpressure, setup, teardown),
        cmocka_unit_test_setup_teardown(test_philosophers_model, setup, teardown),
        cmocka_unit_test_setup_teardown(test_dispatcher_lifecycle, setup, teardown),
        cmocka_unit_test_setup_teardown(test_barrier_senders, setup, teardown),
        cmocka_unit_test_setup_teardown(test_dispatcher_close_race, setup, teardown),
        cmocka_unit_test_setup_teardown(test_runtime_output, setup, teardown),
        cmocka_unit_test_setup_teardown(test_application, setup, teardown),
        cmocka_unit_test_setup_teardown(test_submission, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
