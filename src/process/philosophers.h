/** @file
 * Объявления модуля philosophers.
 */
#ifndef DISTR_PHILOSOPHERS_H
#define DISTR_PHILOSOPHERS_H

#include "process.h"

/** Назначает грязные вилки меньшим ID, маркеры — большим ID (вызывается только в ребёнке). */
void philosophers_initialize(Process* process);

/** Обслуживает запросы вне КС; отложенная отправка допускается.
 * При -1 errno сохраняет ошибку send(). */
int philosophers_service(Process* process);

/** Обновляет состояние по сообщению от другого процесса с sender в пределах [0, count).
 * При -1 errno = EBADMSG: неверный участник, тело, тип или повторная передача ресурса. */
int philosophers_receive(Process* process, local_id sender, const Message* message);

/** Проверяет наличие всех нужных вилок и отсутствие пар «грязная вилка + маркер». */
int philosophers_ready(const Process* process);

/** Ожидает входа в КС (вызывается только в ребёнке с --mutexl в состоянии THINKING).
 * При -1 errno сохраняет ошибку IPC или протокола. */
int request_cs(const void* self);

/** Покидает КС и отдаёт вилки (вызывается только в ребёнке в состоянии EATING).
 * При -1 errno сохраняет ошибку send(). */
int release_cs(const void* self);

/** Выполняет 5 × ID вызовов print() (вызывается только в ребёнке).
 * При -1 errno: EIO — ошибка/усечение snprintf(); иначе ошибка IPC или протокола. */
int perform_work(Process* process);

#endif // DISTR_PHILOSOPHERS_H
