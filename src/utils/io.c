#include "io.h"

int io_write_all(int descriptor, const void* data, size_t size) {
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
