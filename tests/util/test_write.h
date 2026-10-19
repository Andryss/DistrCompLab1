/** @file
 * Объявления модуля test_write.
 */
#ifndef DISTR_TEST_WRITE_H
#define DISTR_TEST_WRITE_H

#include <stddef.h>
#include <sys/types.h>

/** Настраивает ограничение записи и число искусственных EINTR/EAGAIN для дескриптора. */
void test_write_inject(int fd, size_t limit, int interrupts, int blocks);

/** Отключает подмену поведения write() после теста. */
void test_write_reset(void);

/** Выполняет write() с настроенными сбоями; подмена используется только в unit-тестах. */
ssize_t test_write(int fd, const void* bytes, size_t size);

#endif // DISTR_TEST_WRITE_H
