/** @file
 * Объявления модуля channels.
 */
#ifndef DISTR_CHANNELS_H
#define DISTR_CHANNELS_H

#include "platform.h"
#include <fcntl.h>
#include <unistd.h>
#include "logging.h"
#include "process.h"

/** Устанавливает -1 для каждого дескриптора до начала выделения ресурсов. */
void channels_initialize(Process* process);

/** Создаёт и логирует неблокирующий канал для каждой пары различных процессов. */
int channels_create(Process* process, const Logs* logs);

/** После fork() оставляет только входы и выходы текущего процесса. */
void channels_keep_local(Process* process);

/** Закрывает все оставшиеся дескрипторы, в том числе при незавершённой подготовке. */
void channels_close(Process* process);

#endif // DISTR_CHANNELS_H
