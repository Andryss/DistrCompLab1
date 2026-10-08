#ifndef PA1_IO_H
#define PA1_IO_H

#include "platform.h"
#include <errno.h>
#include <unistd.h>
#include <stddef.h>

/**
 * Записывает все байты в блокирующий дескриптор, повторяя прерванные записи.
 * Используется также для файлов и stdout: один write() не гарантирует всю запись.
 * @return 0 при успехе, -1 при ошибке с установленным errno
 */
int io_write_all(int descriptor, const void* data, size_t size);

#endif // PA1_IO_H
