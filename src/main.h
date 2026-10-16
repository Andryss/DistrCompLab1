/** @file
 * Объявления модуля main.
 */
#ifndef DISTR_MAIN_H
#define DISTR_MAIN_H

#include "platform.h"
#include "runtime.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Проверенные параметры запуска программы. */
typedef struct {
    int child_count; /**< Число дочерних процессов без родителя. */
    int mutex_enabled; /**< Признак защиты каждого print() алгоритмом философов. */
} LaunchOptions;

#endif // DISTR_MAIN_H
