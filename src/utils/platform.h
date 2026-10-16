/** @file
 * Объявления модуля platform.
 */
#ifndef DISTR_PLATFORM_H
#define DISTR_PLATFORM_H

/** Открывает объявления POSIX.1-2008 (например, nanosleep и kill) при -std=c99.
 * Должен быть подключён раньше любых системных заголовков. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#endif // DISTR_PLATFORM_H
