#ifndef PA1_TEST_CASES_H
#define PA1_TEST_CASES_H

void test_ipc_setup(void);
void test_ipc_teardown(void);
void test_ipc_wire_format(void** state);
void test_ipc_empty_and_maximum_payload(void** state);
void test_ipc_multicast(void** state);
void test_ipc_partial_reception(void** state);
void test_ipc_round_robin(void** state);
void test_program(const char* binary);
void test_package(const char* repository);

#endif
