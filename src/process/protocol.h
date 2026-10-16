/** @file
 * Объявления модуля protocol.
 */
#ifndef DISTR_PROTOCOL_H
#define DISTR_PROTOCOL_H

#include "platform.h"
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include "logging.h"
#include "process.h"

/** Выполняет фазы STARTED и DONE для ребёнка, включая рассылку и логирование. */
int protocol_run_child(Process* process, const Logs* logs);

/** Получает STARTED и DONE от всех детей; родитель ничего не рассылает. */
int protocol_run_parent(Process* process);

#endif // DISTR_PROTOCOL_H
