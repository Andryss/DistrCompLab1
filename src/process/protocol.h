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

/** Обрабатывает один кадр и обслуживает запросы философов; возвращает 0 либо -1 при ошибке. */
int protocol_step(Process* process);

/** Ожидает STARTED или DONE от всех других детей, продолжая обслуживать IPC. */
int protocol_wait(Process* process, MessageType type);

/** Выполняет стартовый барьер, полезную работу и завершающий барьер ребёнка. */
int protocol_run_child(Process* process, const Logs* logs);

/** Получает STARTED и DONE от всех детей; родитель ничего не рассылает. */
int protocol_run_parent(Process* process);

#endif // DISTR_PROTOCOL_H
