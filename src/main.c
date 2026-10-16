#include "main.h"

/** Проверяет десятичное число детей без знака и посторонних символов. */
static int parse_child_count(const char* number) {
    if (number[0] < '0' || number[0] > '9') {
        return -1;
    }

    char* end;
    errno = 0;
    long child_count = strtol(number, &end, 10);
    if (errno == ERANGE || *end != '\0' || child_count > MAX_PROCESS_ID) {
        return -1;
    }
    return (int)child_count;
}

/** Разбирает параметры в любом порядке, отклоняя повторения и неизвестные ключи. */
static int parse_arguments(int argc, char** argv, LaunchOptions* options) {
    options->child_count = -1;
    options->mutex_enabled = 0;

    for (int index = 1; index < argc; ++index) {
        if (strcmp(argv[index], "--mutexl") == 0) {
            if (options->mutex_enabled) {
                return -1;
            }
            options->mutex_enabled = 1;
        } else if (strcmp(argv[index], "-p") == 0) {
            if (options->child_count >= 0 || index + 1 >= argc) {
                return -1;
            }
            options->child_count = parse_child_count(argv[++index]);
            if (options->child_count < 0) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    return options->child_count >= 0 ? 0 : -1;
}

/** Проверяет командную строку и запускает распределённую систему. */
int main(int argc, char** argv) {
    LaunchOptions options;
    if (parse_arguments(argc, argv, &options) < 0) {
        fprintf(stderr, "Usage: %s -p X [--mutexl] (0 <= X <= %d)\n", argv[0], MAX_PROCESS_ID);
        return EXIT_FAILURE;
    }
    if (runtime_execute(options.child_count, options.mutex_enabled) < 0) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
