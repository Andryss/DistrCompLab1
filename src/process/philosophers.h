/** @file
 * Объявления модуля philosophers.
 */
#ifndef DISTR_PHILOSOPHERS_H
#define DISTR_PHILOSOPHERS_H

#include "process.h"

/** Распределяет грязные вилки меньшим ID, маркеры запросов — большим ID. */
void philosophers_initialize(Process* process);

/** Обслуживает запросы вне КС; возвращает 0 при успехе, -1 при ошибке отправки. */
int philosophers_service(Process* process);

/** Принимает маркер или вилку; возвращает -1 при нарушении протокола либо ошибке IPC. */
int philosophers_receive(Process* process, local_id sender, const Message* message);

/** Проверяет наличие всех нужных вилок и отсутствие пар «грязная вилка + маркер». */
int philosophers_ready(const Process* process);

/** Ожидает входа в КС, обслуживая IPC; возвращает 0 при входе, -1 при ошибке. */
int request_cs(const void* self);

/** Покидает КС и отдаёт запрошенные грязные вилки; возвращает 0 либо -1 при ошибке. */
int release_cs(const void* self);

/** Выполняет 5 × ID итераций print(), обслуживая обмен между итерациями. */
int perform_work(Process* process);

#endif // DISTR_PHILOSOPHERS_H
