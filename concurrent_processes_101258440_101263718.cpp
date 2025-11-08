#include <iostream>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <cstdlib>

/**
 * @author Adam Rebello & Marc Aoun 
 *
 */


// shared structure for shared memory variables
struct SharedData {
    int multiple;  // value of multiple 
    int counter;   // value of counter used by Process 1 
};

// union needed for semaphore operations
union SemaphoreUnion {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// semaphore wait (P) operation - locks semaphore
void sem_wait(int semid) {
    struct sembuf sb = {0, -1, 0};
    semop(semid, &sb, 1);
}

// semaphore signal (V) operation - unlocks semaphore
void sem_signal(int semid) {
    struct sembuf sb = {0, 1, 0};
    semop(semid, &sb, 1);
}

int main() {
    key_t key = ftok("shmfile", 65);  

    int shmid = shmget(key, sizeof(SharedData), 0666 | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget failed");
        exit(EXIT_FAILURE);
    }

    SharedData *shm = (SharedData *)shmat(shmid, nullptr, 0);
    if (shm == (void *)-1) {
        perror("shmat failed");
        exit(EXIT_FAILURE);
    }

    int semid = semget(key, 1, 0666 | IPC_CREAT);
    if (semid < 0) {
        perror("semget failed");
        exit(EXIT_FAILURE);
    }

    SemaphoreUnion sem_val;
    sem_val.val = 1;
    if (semctl(semid, 0, SETVAL, sem_val) < 0) {
        perror("semctl SETVAL failed");
        exit(EXIT_FAILURE);
    }

    // initialize shared variables
    shm->multiple = 3;
    shm->counter = 0;

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // child process - decrements counter, starts after counter > 100
        while (true) {
            sem_wait(semid);
            int counter = shm->counter;
            int multiple = shm->multiple;
            sem_signal(semid);

            if (counter <= 100) {
                std::cout << "child waiting for counter > 100: current = " << counter << std::endl;
                continue;
            }

            if (counter > 500) break;

            if (counter % multiple == 0)
                std::cout << "child: " << counter << " is a multiple of " << multiple << std::endl;
            else
                std::cout << "child: counter = " << counter << std::endl;

            sem_wait(semid);
            shm->counter--;
            sem_signal(semid);

        }
        shmdt(shm);
        exit(0);
    } else {
        // parent process - increments counter, stops at 500
        while (true) {
            sem_wait(semid);
            int counter = shm->counter;
            int multiple = shm->multiple;
            sem_signal(semid);

            if (counter > 500) break;

            if (counter % multiple == 0)
                std::cout << "Parent: " << counter << " is a multiple of " << multiple << std::endl;
            else
                std::cout << "Parent: counter = " << counter << std::endl;

            sem_wait(semid);
            shm->counter++;
            sem_signal(semid);

            usleep(500000);
        }
        wait(nullptr);

        shmdt(shm);
        shmctl(shmid, IPC_RMID, nullptr);
        semctl(semid, 0, IPC_RMID);
    }

    return 0;
}

