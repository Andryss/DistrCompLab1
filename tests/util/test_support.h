/** @file
 * Объявления модуля test_support.
 */
#ifndef DISTR_TEST_SUPPORT_H
#define DISTR_TEST_SUPPORT_H

/* В macOS mkdtemp объявляется в расширениях Darwin, а не в POSIX-разделе. */
#ifdef __APPLE__
#define _DARWIN_C_SOURCE
#endif

#include "platform.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/** Создаёт временный каталог для всех запусков набора тестов. */
void test_workspace_open(void);
/** Рекурсивно удаляет только временный каталог текущего набора. */
void test_workspace_close(void);
/** Создаёт отдельный каталог для следующего тестового запуска. */
void test_case_directory(char path[PATH_MAX]);
/** Соединяет путь и имя, проверяя отсутствие усечения. */
void test_path(char result[PATH_MAX], const char* directory, const char* name);
/** Выполняет команду в отдельной группе процессов, проверяя тайм-аут и очистку детей. */
int test_command(char* const arguments[], const char* directory, int timeout);
/** Читает файл в выделенную строку; освобождение возложено на вызывающий код. */
char* test_read_file(const char* path);
/** Читает stdout последней команды. */
char* test_stdout(void);
/** Читает stderr последней команды. */
char* test_stderr(void);

#endif // DISTR_TEST_SUPPORT_H
