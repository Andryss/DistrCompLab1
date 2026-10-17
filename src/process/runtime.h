/** @file
 * Объявления модуля runtime.
 */
#ifndef DISTR_RUNTIME_H
#define DISTR_RUNTIME_H

#include "platform.h"
#include "process.h"
#include "channels.h"
#include "logging.h"
#include "protocol.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/** Запускает процессы и освобождает ресурсы; параметры проверены при разборе CLI.
 * Возвращает 0 либо -1 с диагностикой в stderr. */
int runtime_execute(int child_count, int mutex_enabled);

#endif // DISTR_RUNTIME_H
