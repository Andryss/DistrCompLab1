#include "platform.h"
#include "process.h"
#include "test_cases.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <cmocka.h>

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static Process peers[3];

/** Создаёт чистые контексты для каждого теста; дескрипторами владеет fixture. */
void test_ipc_setup(void) {
    memset(peers, 0, sizeof(peers));
    for (int id = 0; id < 3; ++id) {
        peers[id].id = (local_id)id;
        peers[id].count = 3;
        for (int other = 0; other < PROCESS_LIMIT; ++other) {
            peers[id].in[other] = -1;
            peers[id].out[other] = -1;
        }
    }
}

/** Закрывает каналы также после неуспешного утверждения cmocka. */
void test_ipc_teardown(void) {
    for (int id = 0; id < 3; ++id) {
        for (int other = 0; other < PROCESS_LIMIT; ++other) {
            if (peers[id].in[other] >= 0) {
                close(peers[id].in[other]);
            }
            if (peers[id].out[other] >= 0) {
                close(peers[id].out[other]);
            }
        }
    }
}

/** Соединяет контексты настоящим pipe с неблокирующим чтением. */
static void connect_peers(int sender, int receiver) {
    int descriptors[2];
    assert_int_equal(0, pipe(descriptors));
    peers[sender].out[receiver] = descriptors[1];
    peers[receiver].in[sender] = descriptors[0];
    int flags = fcntl(descriptors[0], F_GETFL);
    assert_true(flags >= 0);
    assert_int_equal(0, fcntl(descriptors[0], F_SETFL, flags | O_NONBLOCK));
}

/** Заполняет неиспользуемый хвост, чтобы тест обнаружил его случайную передачу. */
static Message make_message(const char* text) {
    Message message;
    memset(&message, 0x5a, sizeof(message));
    message.s_header.s_magic = MESSAGE_MAGIC;
    message.s_header.s_payload_len = (uint16_t)strlen(text);
    message.s_header.s_type = STARTED;
    message.s_header.s_local_time = 0;
    memcpy(message.s_payload, text, strlen(text));
    return message;
}

/** Проверяет длину и байты тела без предположения о завершающем NUL. */
static void assert_payload(const Message* message, const char* text) {
    assert_int_equal(MESSAGE_MAGIC, message->s_header.s_magic);
    assert_int_equal(strlen(text), message->s_header.s_payload_len);
    if (strlen(text) > 0) {
        assert_memory_equal(text, message->s_payload, strlen(text));
    }
}

void test_ipc_wire_format(void** state) {
    (void)state;
    connect_peers(0, 1);
    Message message = make_message("hello\n");
    unsigned char wire[MAX_MESSAGE_LEN];

    assert_int_equal(0, send(&peers[0], 1, &message));
    ssize_t length = read(peers[1].in[0], wire, sizeof(wire));

    assert_int_equal(sizeof(MessageHeader) + 6, length);
    assert_memory_equal(&message, wire, (size_t)length);
}

void test_ipc_empty_and_maximum_payload(void** state) {
    (void)state;
    connect_peers(0, 1);
    Message message = make_message("");
    Message received;
    assert_int_equal(0, send(&peers[0], 1, &message));
    assert_int_equal(0, receive(&peers[1], 0, &received));
    assert_payload(&received, "");
    message.s_header.s_payload_len = MAX_PAYLOAD_LEN;
    memset(message.s_payload, 'x', MAX_PAYLOAD_LEN);
    assert_int_equal(0, send(&peers[0], 1, &message));

    assert_int_equal(0, receive(&peers[1], 0, &received));
    assert_int_equal(MAX_PAYLOAD_LEN, received.s_header.s_payload_len);
    assert_memory_equal(message.s_payload, received.s_payload, MAX_PAYLOAD_LEN);
}

void test_ipc_multicast(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(1, 2);
    Message message = make_message("all");
    Message received;

    assert_int_equal(0, send_multicast(&peers[1], &message));

    assert_int_equal(0, receive(&peers[0], 1, &received));
    assert_payload(&received, "all");
    assert_int_equal(0, receive(&peers[2], 1, &received));
    assert_payload(&received, "all");
}

void test_ipc_partial_reception(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(2, 0);
    Message partial = make_message("fragment");
    Message ready = make_message("ready");
    Message received;

    assert_int_equal(0, send(&peers[2], 0, &ready));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "ready");

    assert_int_equal(3, write(peers[1].out[0], &partial, 3));
    assert_int_equal(0, send(&peers[2], 0, &ready));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "ready");

    size_t fragment = sizeof(MessageHeader) - 3 + 2;
    assert_int_equal(fragment, write(peers[1].out[0], (char*)&partial + 3, fragment));
    assert_int_equal(0, send(&peers[2], 0, &ready));
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "ready");

    assert_int_equal(6, write(peers[1].out[0], partial.s_payload + 2, 6));
    assert_int_equal(0, receive(&peers[0], 1, &received));
    assert_payload(&received, "fragment");
}

void test_ipc_round_robin(void** state) {
    (void)state;
    connect_peers(1, 0);
    connect_peers(2, 0);
    Message busy = make_message("busy");
    Message other = make_message("other");
    Message received;
    assert_int_equal(0, send(&peers[1], 0, &busy));
    assert_int_equal(0, send(&peers[1], 0, &busy));
    assert_int_equal(0, send(&peers[2], 0, &other));

    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "busy");
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "other");
    assert_int_equal(0, receive_any(&peers[0], &received));
    assert_payload(&received, "busy");
}
