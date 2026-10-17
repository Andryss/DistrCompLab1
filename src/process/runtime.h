/** @file
 * Объявления модуля runtime.
 */
#ifndef DISTR_RUNTIME_H
#define DISTR_RUNTIME_H

/** Запускает систему и собирает детей (вызывается только в родителе).
 * При -1 errno сохраняет ошибку запуска, протокола, ожидания или логов. */
int runtime_execute(int child_count, int mutex_enabled);

#endif // DISTR_RUNTIME_H
