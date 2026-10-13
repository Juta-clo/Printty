#include "unity_fixture.h"
#include "pty_test.h"

void run_all(void) {
    RUN_TEST_GROUP(input);
    RUN_TEST_GROUP(event);
    RUN_TEST_GROUP(csi);
}

int main(int argc, const char **argv) {
    init_harness();
    return UnityMain(argc, argv, run_all);
}
