#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>

const int LIMIT = 1000;

struct shared_t {
    sem_t sem;
    int   value;
};

static int coin() {
    return std::rand() % 2 + 1;
}

static void worker(shared_t *shm, const char *name) {
    std::srand(static_cast<unsigned>(std::time(nullptr)) ^
               static_cast<unsigned>(getpid()));

    for (;;) {
        sem_wait(&shm->sem);

        int v = shm->value;        
        if (v >= LIMIT) {
            sem_post(&shm->sem);
            break;
        }

        while (v < LIMIT && coin() == 2) { 
            v++;
            shm->value = v;          
            std::printf("[%s] a scris %d\n", name, v);
        }
        std::fflush(stdout);

        sem_post(&shm->sem);
        usleep(100);
    }
}

int main() {
    void *mem = mmap(nullptr, sizeof(shared_t),
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { std::perror("mmap"); return 1; }

    shared_t *shm = static_cast<shared_t *>(mem);
    shm->value = 0;

    if (sem_init(&shm->sem, 1, 1) == -1) { std::perror("sem_init"); return 1; }

    const char *names[2] = {"Proces A", "Proces B"};
    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid == -1) { std::perror("fork"); return 1; }
        if (pid == 0) {
            worker(shm, names[i]);
            _exit(0);
        }
    }

    wait(nullptr);
    wait(nullptr);

    std::printf("Valoare finala: %d\n", shm->value);

    sem_destroy(&shm->sem);
    munmap(shm, sizeof(shared_t));
    return 0;
}
