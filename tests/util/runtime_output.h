/** @file
 * Проверка посимвольного вывода оригинальной библиотеки.
 */
#ifndef DISTR_RUNTIME_OUTPUT_H
#define DISTR_RUNTIME_OUTPUT_H

/** Проверяет состав символов; при mutex_enabled также проверяет целостность и порядок строк. */
int test_runtime_output_valid(const char* output, int children, int mutex_enabled);

#endif // DISTR_RUNTIME_OUTPUT_H
