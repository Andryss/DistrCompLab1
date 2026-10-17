/** @file
 * Объявления модуля channels.
 */
#ifndef DISTR_CHANNELS_H
#define DISTR_CHANNELS_H

#include "process.h"
#include "logging.h"

/** Инициализирует каналы и контекст приёма (вызывается только в родителе до fork()). */
void channels_initialize(Process* process);

/** Создаёт и логирует каналы (вызывается только в родителе до fork()).
 * При -1 errno сохраняет ошибку pipe(), fcntl() или логирования. */
int channels_create(Process* process, const Logs* logs);

/** Оставляет только свои входы и выходы (вызывается в каждом процессе после fork()). */
void channels_keep_local(Process* process);

/** Закрывает все оставшиеся дескрипторы, в том числе при незавершённой подготовке. */
void channels_close(Process* process);

#endif // DISTR_CHANNELS_H
