#ifndef PA1_PLATFORM_H
#define PA1_PLATFORM_H

/** Открывает объявления POSIX.1-2008 (например, nanosleep и kill) при -std=c99.
 * Должен быть подключён раньше любых системных заголовков. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#endif // PA1_PLATFORM_H
