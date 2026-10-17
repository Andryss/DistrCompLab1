#include "test_support.h"
#include "test_cases.h"
#include "test_write.h"
#include "philosophers.h"
#include "protocol.h"
#include "philosophers_model.h"

void test_philosophers_rules(void** state) {
    (void)state;
    test_model_open(3);
    Process* low = test_model_process(1);
    Process* high = test_model_process(2);
    assert_true(low->philosopher.fork[2] && low->philosopher.dirty[2] && !low->philosopher.req[2]);
    assert_true(!high->philosopher.fork[1] && high->philosopher.req[1]);
    high->philosopher.state = HUNGRY;
    assert_int_equal(0, philosophers_service(high));
    assert_false(high->philosopher.req[1]);
    assert_int_equal(0, philosophers_service(high)); /* no duplicate request */
    Message message;
    assert_int_equal(0, receive_any(low, &message));
    assert_int_equal(CS_REQUEST, message.s_header.s_type);
    assert_int_equal(-1, receive_any(low, &message));
    assert_int_equal(EAGAIN, errno);

    low->philosopher.state = EATING;
    message = test_control_message(CS_REQUEST);
    assert_int_equal(0, philosophers_receive(low, 2, &message));
    assert_int_equal(0, philosophers_service(low));
    assert_true(low->philosopher.fork[2] && low->philosopher.req[2]); /* cannot give while eating */
    assert_false(philosophers_ready(low)); /* dirty + request blocks re-entry */
    assert_int_equal(0, release_cs(low));
    assert_false(low->philosopher.fork[2]);
    assert_int_equal(0, receive_any(high, &message));
    assert_int_equal(CS_REPLY, message.s_header.s_type);
    assert_int_equal(0, philosophers_receive(high, 1, &message));
    assert_true(high->philosopher.fork[1] && !high->philosopher.dirty[1]);

    low->philosopher.state = HUNGRY;
    assert_int_equal(0, philosophers_service(low));
    assert_int_equal(0, receive_any(high, &message));
    assert_int_equal(0, philosophers_receive(high, 1, &message));
    assert_true(high->philosopher.req[1] && high->philosopher.fork[1] && !high->philosopher.dirty[1]);
    assert_true(philosophers_ready(high)); /* clean fork kept despite request */
    high->philosopher.state = THINKING;
    assert_int_equal(0, request_cs(high));
    assert_true(high->philosopher.state == EATING && high->philosopher.dirty[1]);
    assert_int_equal(0, release_cs(high));
    assert_false(high->philosopher.fork[1]);
    assert_int_equal(0, receive_any(low, &message));
    assert_int_equal(0, philosophers_receive(low, 2, &message));
    assert_true(philosophers_ready(low));
    test_model_close();
}

void test_philosophers_model(void** state) {
    (void)state;
    for (int schedule = 0; schedule < 8; ++schedule) {
        test_model_run(schedule);
    }
}

/** DONE не удаляет ребро: закончивший работу философ продолжает отдавать вилки. */
static void check_service_after_done(void) {
    test_model_open(3);
    Process* low = test_model_process(1);
    Process* high = test_model_process(2);
    high->philosopher.state = HUNGRY;
    assert_int_equal(0, philosophers_service(high));
    assert_int_equal(0, protocol_step(low));
    assert_int_equal(0, protocol_step(high));
    high->philosopher.state = THINKING;
    assert_int_equal(0, request_cs(high));
    assert_int_equal(0, release_cs(high));

    Message started = test_control_message(STARTED);
    assert_int_equal(0, send(high, 1, &started));
    assert_int_equal(0, protocol_wait(low, STARTED));
    Message done = test_control_message(DONE);
    assert_int_equal(0, send(high, 1, &done));
    assert_int_equal(0, protocol_step(low));
    assert_int_equal(1, low->barriers.done[2]);
    assert_false(philosophers_ready(low));

    low->philosopher.state = HUNGRY;
    assert_int_equal(0, philosophers_service(low));
    assert_int_equal(0, protocol_step(high));
    assert_int_equal(THINKING, high->philosopher.state);
    assert_int_equal(0, protocol_step(low));
    assert_true(philosophers_ready(low));
    test_model_close();
}

void test_protocol_lifecycle(void** state) {
    (void)state;
    check_service_after_done();
    test_model_open(3);
    Message message = test_control_message(STARTED);
    assert_int_equal(0, send(test_model_process(2), 1, &message));
    message = test_control_message(CS_REQUEST);
    assert_int_equal(0, send(test_model_process(2), 1, &message));
    message = test_control_message(DONE);
    assert_int_equal(0, send(test_model_process(2), 1, &message));
    assert_int_equal(0, protocol_wait(test_model_process(1), STARTED));
    assert_int_equal(0, protocol_step(test_model_process(1)));
    Message reply;
    assert_int_equal(0, receive_any(test_model_process(2), &reply));
    assert_int_equal(CS_REPLY, reply.s_header.s_type);
    /* Early DONE is recorded even before the caller enters its DONE barrier. */
    assert_int_equal(0, protocol_step(test_model_process(1)));
    assert_int_equal(1, test_model_process(1)->barriers.done[2]);
    assert_int_equal(0, protocol_wait(test_model_process(1), DONE));
    close(test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE]);
    test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE] = -1;
    assert_int_equal(0, protocol_step(test_model_process(1))); /* EOF after DONE */
    test_model_close();

    test_model_open(3);
    close(test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE]);
    test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE] = -1;
    assert_int_equal(-1, protocol_step(test_model_process(1)));
    assert_int_equal(EPIPE, errno);
    test_model_close();
}


void test_protocol_close_race(void** state) {
    (void)state;
    test_model_open(3);
    assert_true(signal(SIGPIPE, SIG_IGN) != SIG_ERR);
    test_model_process(1)->barriers.started[2] = 1;
    Message request = test_control_message(CS_REQUEST), done = test_control_message(DONE);
    assert_int_equal(0, send(test_model_process(2), 1, &request));
    assert_int_equal(0, send(test_model_process(2), 1, &done));
    /* Neighbour closed after its DONE, which is still behind its old request. */
    close(test_model_process(2)->ipc.descriptors[1][2][PIPE_READ]);
    test_model_process(2)->ipc.descriptors[1][2][PIPE_READ] = -1;
    close(test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE]);
    test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE] = -1;
    assert_int_equal(0, protocol_step(test_model_process(1)));
    assert_int_equal(0, protocol_wait(test_model_process(1), DONE));
    test_model_close();

    test_model_open(3);
    assert_int_equal(0, send(test_model_process(2), 1, &request));
    close(test_model_process(2)->ipc.descriptors[1][2][PIPE_READ]);
    test_model_process(2)->ipc.descriptors[1][2][PIPE_READ] = -1;
    close(test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE]);
    test_model_process(2)->ipc.descriptors[2][1][PIPE_WRITE] = -1;
    assert_int_equal(0, protocol_step(test_model_process(1)));
    assert_int_equal(-1, protocol_step(test_model_process(1)));
    assert_int_equal(EPIPE, errno); /* absence of DONE must never be masked */
    Message lifecycle = test_control_message(DONE);
    assert_int_equal(-1, send(test_model_process(1), 2, &lifecycle));
    assert_int_equal(EPIPE, errno); /* lifecycle delivery is mandatory */
    test_model_close();
}

void test_philosophers_backpressure(void** state) {
    (void)state;
    test_model_open(3);
    Process* low = test_model_process(1);
    Process* high = test_model_process(2);
    high->philosopher.state = HUNGRY;

    // Непринятый запрос не должен забирать локальный маркер.
    test_write_inject(high->ipc.descriptors[high->id][1][PIPE_WRITE], MAX_MESSAGE_LEN, 0, 1);
    assert_int_equal(0, philosophers_service(high));
    assert_true(high->philosopher.req[1]);
    assert_int_equal(0, philosophers_service(high));
    assert_false(high->philosopher.req[1]);
    test_write_reset();

    Message message;
    assert_int_equal(0, receive_any(low, &message));

    // Грязная вилка остаётся у владельца до начала успешной передачи.
    test_write_inject(low->ipc.descriptors[low->id][2][PIPE_WRITE], MAX_MESSAGE_LEN, 0, 1);
    assert_int_equal(0, philosophers_receive(low, low->ipc.receive.sender, &message));
    assert_int_equal(0, philosophers_service(low));
    assert_true(low->philosopher.fork[2] && low->philosopher.req[2]);
    assert_false(philosophers_ready(low));
    assert_int_equal(0, philosophers_service(low));
    assert_false(low->philosopher.fork[2]);
    test_write_reset();

    assert_int_equal(0, receive_any(high, &message));
    assert_int_equal(0, philosophers_receive(high, high->ipc.receive.sender, &message));
    assert_true(high->philosopher.fork[1] && !high->philosopher.dirty[1]);
    assert_true(philosophers_ready(high));
    test_model_close();
}

void test_barrier_senders(void** state) {
    (void)state;
    MessageType types[] = {STARTED, DONE};
    for (size_t index = 0; index < sizeof(types) / sizeof(types[0]); ++index) {
        test_model_open(4);
        Process* process = test_model_process(1);
        if (types[index] == DONE) {
            process->barriers.started[2] = 1;
            process->barriers.started[3] = 1;
        }
        Message message = test_control_message(types[index]);
        assert_int_equal(0, send(test_model_process(2), 1, &message));
        assert_int_equal(0, protocol_step(process));
        const int* received = types[index] == STARTED ? process->barriers.started : process->barriers.done;
        assert_true(received[2]);
        assert_false(received[3]);

        // Повтор от ребёнка 2 не заменяет отсутствующее событие ребёнка 3.
        assert_int_equal(0, send(test_model_process(2), 1, &message));
        assert_int_equal(0, protocol_step(process));
        assert_true(received[2]);
        assert_false(received[3]);

        assert_int_equal(0, send(test_model_process(3), 1, &message));
        assert_int_equal(0, protocol_wait(process, types[index]));
        assert_true(received[2] && received[3]);
        test_model_close();
    }

    Message done = test_control_message(DONE);
    // DONE одного ребёнка не разрешает закрытие канала другого.
    test_model_open(4);
    Process* process = test_model_process(1);
    process->barriers.started[2] = 1;
    assert_int_equal(0, send(test_model_process(2), 1, &done));
    assert_int_equal(0, protocol_step(process));
    close(test_model_process(3)->ipc.descriptors[3][1][PIPE_WRITE]);
    test_model_process(3)->ipc.descriptors[3][1][PIPE_WRITE] = -1;
    assert_int_equal(-1, protocol_step(process));
    assert_int_equal(EPIPE, errno);
    test_model_close();
}

void test_philosophers_receive_only(void** state) {
    (void)state;
    test_model_open(3);
    Process* low = test_model_process(1);
    Process* high = test_model_process(2);
    high->philosopher.state = HUNGRY;
    assert_int_equal(0, philosophers_service(high));
    Message message;
    assert_int_equal(0, receive_any(low, &message));
    assert_int_equal(0, philosophers_receive(low, low->ipc.receive.sender, &message));
    assert_true(low->philosopher.fork[2] && low->philosopher.req[2]);
    assert_int_equal(-1, receive_any(high, &message));
    assert_int_equal(EAGAIN, errno);

    assert_int_equal(0, philosophers_service(low));
    assert_int_equal(0, receive_any(high, &message));
    assert_int_equal(CS_REPLY, message.s_header.s_type);
    test_model_close();

    // Один шаг диспетчера делает одну попытку обслуживания после приёма.
    test_model_open(3);
    low = test_model_process(1);
    high = test_model_process(2);
    high->philosopher.state = HUNGRY;
    assert_int_equal(0, philosophers_service(high));
    test_write_inject(low->ipc.descriptors[low->id][2][PIPE_WRITE], MAX_MESSAGE_LEN, 0, 1);
    assert_int_equal(0, protocol_step(low));
    assert_true(low->philosopher.fork[2] && low->philosopher.req[2]);
    assert_int_equal(-1, receive_any(high, &message));
    assert_int_equal(EAGAIN, errno);
    assert_int_equal(0, protocol_step(low));
    assert_false(low->philosopher.fork[2]);
    assert_int_equal(0, receive_any(high, &message));
    test_write_reset();
    test_model_close();
}
