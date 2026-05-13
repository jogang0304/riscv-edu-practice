#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>

#define __NR_get_mcycle_from_sbi 471

int main() {
    long mcycle_val;

    mcycle_val = syscall(__NR_get_mcycle_from_sbi);

    if (mcycle_val < 0) {
        perror("Syscall failed");
        return 1;
    }

    printf("mcycle value: %ld\n", mcycle_val);
    return 0;
}
