/** @file
 * Объявления модуля test_cases.
 */
#ifndef DISTR_TEST_CASES_H
#define DISTR_TEST_CASES_H

/** Подготавливает контексты IPC перед тестом. */
void test_ipc_setup(void);

/** Закрывает каналы после теста IPC. */
void test_ipc_teardown(void);

/** Проверяет точные байты и длину кадра в канале. */
void test_ipc_wire_format(void** state);

/** Проверяет пустое и максимально допустимое тело сообщения. */
void test_ipc_empty_and_maximum_payload(void** state);

/** Проверяет рассылку всем процессам, включая родителя. */
void test_ipc_multicast(void** state);

/** Проверяет дочитывание заголовка и тела с задержкой в пределах одного вызова. */
void test_ipc_partial_reception(void** state);

/** Проверяет EAGAIN при пустом канале и порядок готовых сообщений. */
void test_ipc_receive_pending(void** state);

/** Проверяет пустые каналы и различение EAGAIN/EOF при приёме от любого отправителя. */
void test_ipc_receive_any_pending(void** state);

/** Проверяет справедливый круговой обход отправителей. */
void test_ipc_round_robin(void** state);

/** Проверяет FIFO и другие каналы при переполнении одного pipe. */
void test_ipc_backpressure(void** state);

/** Проверяет атомарную запись, предел размера, EINTR и EAGAIN. */
void test_ipc_atomic_writes(void** state);

/** Проверяет недопустимый формат и EOF посреди кадра. */
void test_ipc_errors(void** state);

/** Проверяет O_NONBLOCK на чтении и записи после создания и подключения каналов. */
void test_channels_nonblocking(void** state);

/** Проверяет очистку незавершённой подготовки и безопасность повторного закрытия. */
void test_channels_cleanup(void** state);

/** Проверяет правила владения вилками и маркерами. */
void test_philosophers_rules(void** state);

/** Проверяет сохранение ресурсов, взаимное исключение и обслуживание запросов в модели. */
void test_philosophers_model(void** state);

/** Проверяет события жизненного цикла и обслуживание вилок после своей работы. */
void test_protocol_lifecycle(void** state);

/** Проверяет всех участников барьера, безвредность повторов и закрытие конкретного соседа. */
void test_barrier_senders(void** state);

/** Проверяет гонку закрытия с ранним запросом и ещё не обработанным DONE. */
void test_protocol_close_race(void** state);

/** Проверяет программу в обоих режимах с отметками тестовой print(). */
void test_program(const char* binary);

/** Проверяет состав архива, сборку и запуск с тестовой либо оригинальной библиотекой. */
void test_package(const char* repository, int original_runtime);


/** Проверяет полноту и порядок вывода оригинальной библиотеки. */
void test_runtime_output(void** state);

/** Запускает оба режима с оригинальной Linux-библиотекой, учитывая вывод print() в stderr. */
void test_program_original(const char* binary);

/** Проверяет сохранение вилки и маркера при EAGAIN до начала передачи. */
void test_philosophers_backpressure(void** state);

/** Проверяет однократное событие EOF и работу оставшихся каналов. */
void test_ipc_closed_channels(void** state);

/** Проверяет отсутствие отправок при приёме и однократное обслуживание в диспетчере. */
void test_philosophers_receive_only(void** state);

#endif
