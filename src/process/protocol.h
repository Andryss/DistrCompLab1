/** @file
 * Объявления модуля protocol.
 */
#ifndef DISTR_PROTOCOL_H
#define DISTR_PROTOCOL_H

#include "process.h"
#include "logging.h"

/** Обрабатывает доступный кадр и запросы (вызывается в родителе и детях).
 * При -1 errno: EPIPE — закрытие до DONE ребёнка, EBADMSG — нарушение протокола; иначе ошибка IPC; EAGAIN/EWOULDBLOCK допустимы. */
int protocol_step(Process* process);

/** Ожидает STARTED/DONE от всех других детей.
 * При -1 errno сохраняет ошибку protocol_step(). */
int protocol_wait(Process* process, MessageType type);

/** Проходит барьеры и выполняет полезную работу (вызывается только в ребёнке).
 * При -1 errno сохраняет ошибку IPC, протокола или логирования. */
int protocol_run_child(Process* process, const Logs* logs);

/** Ожидает STARTED/DONE от всех детей (вызывается только в родителе).
 * При -1 errno сохраняет ошибку protocol_wait(). */
int protocol_run_parent(Process* process);

#endif // DISTR_PROTOCOL_H
