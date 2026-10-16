/** @file
 * Объявления модуля dispatcher.
 */
#ifndef DISTR_DISPATCHER_H
#define DISTR_DISPATCHER_H

#include "process.h"

/** Обрабатывает один кадр и обслуживает запросы философов; возвращает 0 либо -1 при ошибке. */
int dispatcher_step(Process* process);

/** Ожидает STARTED или DONE от всех других детей, продолжая обслуживать IPC. */
int dispatcher_wait(Process* process, MessageType type);

#endif // DISTR_DISPATCHER_H
