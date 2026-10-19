/* Only the unit-test executable remaps write; the application uses the real syscall. */
#undef write
#include <unistd.h>
#include <errno.h>
#include "test_write.h"

static int target = -1;
static size_t chunk;
static int interrupted, blocked;

void test_write_inject(int fd, size_t limit, int interrupts, int blocks) {
    target = fd;
    chunk = limit;
    interrupted = interrupts;
    blocked = blocks;
}

void test_write_reset(void) {
    target = -1;
}

ssize_t test_write(int fd, const void* bytes, size_t size) {
    if (fd == target) {
        if (interrupted > 0) {
            --interrupted;
            errno = EINTR; // Имитируем прерывание до записи байтов.
            return -1;
        }
        if (blocked > 0) {
            --blocked;
            errno = EAGAIN; // Имитируем заполненный канал без частичной записи.
            return -1;
        }
        if (size > chunk) {
            size = chunk;
        }
    }
    return write(fd, bytes, size);
}
