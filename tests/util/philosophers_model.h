/** @file
 * Управляемая модель философов для модульных проверок.
 */
#ifndef DISTR_PHILOSOPHERS_MODEL_H
#define DISTR_PHILOSOPHERS_MODEL_H

#include "process.h"

/** Создаёт модель с указанным числом процессов, включая исключённого родителя. */
void test_model_open(int count);

/** Возвращает контекст ребёнка по его локальному ID. */
Process* test_model_process(int id);

/** Закрывает каналы модели; допускает повторный вызов. */
void test_model_close(void);

/** Создаёт пустое управляющее сообщение заданного типа. */
Message test_control_message(int type);

/** Выполняет один порядок доставки, проверяя инварианты и обслуживание всех запросов. */
void test_model_run(int schedule);

#endif // DISTR_PHILOSOPHERS_MODEL_H
