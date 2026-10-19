/** @file
 * Объявления модуля logging.
 */
#ifndef DISTR_LOGGING_H
#define DISTR_LOGGING_H

#include <stddef.h>

/** Файлы логов, открытые родителем и наследуемые дочерними процессами. */
typedef struct {
    int events_fd; /**< Дескриптор events.log; -1, если файл не открыт. */
    int pipes_fd; /**< Дескриптор pipes.log; закрывается до создания детей. */
} Logs;

/** Открывает логи (вызывается только в родителе до fork()).
 * При -1 errno сохраняет ошибку open(); открытые файлы нужно закрыть через logs_close(). */
int logs_open(Logs* logs);

/** Закрывает лог топологии (вызывается только в родителе до fork()).
 * При -1 errno сохраняет ошибку close(). */
int logs_close_pipes(Logs* logs);

/** Закрывает свои дескрипторы логов (вызывается в каждом процессе).
 * При -1 errno сохраняет первую ошибку close(). */
int logs_close(Logs* logs);

/** Пишет событие в events.log и stdout.
 * При -1 errno: EIO — запись 0 байт; иначе ошибка write(). */
int log_event(const Logs* logs, const char* text, size_t length);

/** Записывает канал в лог топологии (вызывается только в родителе).
 * При -1 errno: EIO — запись 0 байт; иначе ошибка write(). */
int log_channel(const Logs* logs, int source, int destination, int read_fd, int write_fd);

#endif // DISTR_LOGGING_H
