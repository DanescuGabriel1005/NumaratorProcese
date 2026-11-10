#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#define LIMIT 1000

#define SHM_NAME "Local\\CounterShm"
#define SEM_NAME "Local\\CounterSem"

static int coin(void) {
    return rand() % 2 + 1;
}

static void worker(HANDLE sem, volatile int* value, const char* name) {
    srand((unsigned)time(NULL) ^ (unsigned)GetCurrentProcessId());

    for (;;) {
        WaitForSingleObject(sem, INFINITE);

        int v = *value;
        if (v >= LIMIT) {
            ReleaseSemaphore(sem, 1, NULL);
            break;
        }

        while (v < LIMIT && coin() == 2) {
            v++;
            *value = v;
            printf("[Proces %s] a scris %d\n", name, v);
        }
        fflush(stdout);

        ReleaseSemaphore(sem, 1, NULL);
        Sleep(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc >= 3 && strcmp(argv[1], "worker") == 0) {
        HANDLE hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, SHM_NAME);
        HANDLE sem = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, SEM_NAME);
        if (!hMap || !sem) { fprintf(stderr, "open failed\n"); return 1; }

        volatile int* value = (volatile int*)MapViewOfFile(
            hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
        if (!value) { fprintf(stderr, "MapViewOfFile failed\n"); return 1; }

        worker(sem, value, argv[2]);

        UnmapViewOfFile((void*)value);
        CloseHandle(hMap);
        CloseHandle(sem);
        return 0;
    }

    HANDLE hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
        0, sizeof(int), SHM_NAME);
    HANDLE sem = CreateSemaphoreA(NULL, 1, 1, SEM_NAME);
    if (!hMap || !sem) { fprintf(stderr, "create failed\n"); return 1; }

    volatile int* value = (volatile int*)MapViewOfFile(
        hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(int));
    if (!value) { fprintf(stderr, "MapViewOfFile failed\n"); return 1; }

    char exe[MAX_PATH];
    GetModuleFileNameA(NULL, exe, MAX_PATH);

    const char* names[2] = { "A", "B" };
    HANDLE procs[2];
    for (int i = 0; i < 2; i++) {
        char cmd[MAX_PATH + 32];
        snprintf(cmd, sizeof(cmd), "\"%s\" worker %s", exe, names[i]);

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            fprintf(stderr, "CreateProcess failed (%lu)\n", GetLastError());
            return 1;
        }
        CloseHandle(pi.hThread);
        procs[i] = pi.hProcess;
    }

    WaitForMultipleObjects(2, procs, TRUE, INFINITE);
    printf("Valoare finala: %d\n", *value);

    CloseHandle(procs[0]);
    CloseHandle(procs[1]);
    UnmapViewOfFile((void*)value);
    CloseHandle(hMap);
    CloseHandle(sem);
    return 0;
}