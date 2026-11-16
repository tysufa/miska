#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUM_CHILDREN 10
#define WORK_LOOP 100000000

void perform_work(int id) {
  // Simulated workload
  volatile unsigned long i;
  for (i = 0; i < WORK_LOOP; i++)
    ;
  printf("Child %d (PID: %d) finished work.\n", id, getpid());
}

int main() {
  printf("Parent process started (PID: %d)\n", getpid());
  printf("Spawning %d children...\n", NUM_CHILDREN);
  printf("------------------------------------------\n");

  for (int i = 0; i < NUM_CHILDREN; i++) {
    pid_t pid = fork();

    if (pid < 0) {
      perror("Fork failed");
      exit(1);
    } else if (pid == 0) {
      // Inside Child Process
      printf(">>> Child %d created with PID: %d\n", i + 1, getpid());

      // Artificial delay to stagger execution start if needed
      usleep(100000);

      perform_work(i + 1);
      exit(0);
    }
    // Parent continues to next iteration to fork again immediately
  }

  // Parent waits for all children to prevent zombies
  for (int i = 0; i < NUM_CHILDREN; i++) {
    wait(NULL);
  }

  printf("------------------------------------------\n");
  printf("All children have exited. Parent exiting.\n");
  return 0;
}
