#include "platform.h"
#include "pa2345.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/** Дописывает тестовый вывод, повторяя запись после EINTR.
 * При -1 errno: EIO — запись 0 байт; иначе ошибка write(). */
static int io_write_all(int descriptor, const void* data, size_t size) {
    const unsigned char* bytes = data;

    while (size > 0) {
        ssize_t result = write(descriptor, bytes, size);
        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (result == 0) {
            errno = EIO;
            return -1;
        }
        bytes += (size_t)result;
        size -= (size_t)result;
    }

    return 0;
}

/** Пишет одну отметку входа или выхода; ошибка тестового runtime завершает ребёнка. */
static void write_trace_event(int descriptor, char operation, int id) {
    char event[32];
    int length = snprintf(event, sizeof(event), "%c %d\n", operation, id);
    if (length < 0 || (size_t)length >= sizeof(event) ||
        io_write_all(descriptor, event, (size_t)length) < 0) {
        _exit(99);
    }
}

/** Открывает трассу, если тест запросил проверку пересечения интервалов print(). */
static int open_trace(const char* text, int* id) {
    const char* path = getenv("TEST_TRACE");
    if (path == NULL) {
        return -1;
    }

    int descriptor = open(path, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (descriptor < 0 || sscanf(text, "process %d", id) != 1) {
        _exit(99);
    }
    return descriptor;
}

/** Задерживает тестовую print(), увеличивая вероятность заметить пересечение КС. */
static void delay_print(void) {
    struct timespec delay = {0, 1000000L};
    while (nanosleep(&delay, &delay) < 0 && errno == EINTR) {
        // Досыпаем оставшуюся часть после сигнала.
    }
}

/** Тестовая замена runtime; не включается в сдаваемую программу и архив. */
void print(const char* text) {
    int id = 0;
    int descriptor = open_trace(text, &id);
    if (descriptor >= 0) {
        write_trace_event(descriptor, '+', id);
        delay_print();
    }

    if (io_write_all(STDOUT_FILENO, text, strlen(text)) < 0) {
        _exit(99);
    }

    if (descriptor >= 0) {
        write_trace_event(descriptor, '-', id);
        close(descriptor);
    }
}
