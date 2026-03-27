// master.cpp
// Parallel matrix multiplication using System V IPC (message queue, shared memory, semaphores).
// Master forks WORKERS children which pick jobs (row chunks) from the message queue.
// Workers compute rows and send progress updates to master (also via the queue).
//
// Build: g++ -std=c++11 src/master.cpp -o master
// Run: ./master <N> <WORKERS>  (e.g., ./master 400 4)

#include <bits/stdc++.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

using namespace std;

// Message structures
struct jobbuf {
    long mtype; // 1 = job, 2 = sentinel
    int start_row;
    int end_row;
};
struct pmsg {
    long mtype; // 3 = progress
    int delta;  // number of rows completed
};

int msqid = -1, shmid = -1, semid = -1;
int N = 200;
int WORKERS = 4;

void cleanup_ipc() {
    if (msqid != -1) { msgctl(msqid, IPC_RMID, nullptr); msqid = -1; }
    if (shmid != -1) { shmctl(shmid, IPC_RMID, nullptr); shmid = -1; }
    if (semid != -1) { semctl(semid, 0, IPC_RMID); semid = -1; }
}

void on_sigint(int) {
    cerr << "\nSIGINT received: cleaning IPC objects...\n";
    cleanup_ipc();
    exit(1);
}

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Helper: write progress JSON (safe: master only writes)
void write_progress_json(int rows_completed, int total_rows) {
    FILE* f = fopen("gui/progress.json", "w");
    if (!f) return;
    fprintf(f, "{ \"status\":\"running\", \"rows_completed\": %d, \"total_rows\": %d }\n", rows_completed, total_rows);
    fclose(f);
}

// Helper: write results JSON
void write_results_json(double* C, int N) {
    FILE* f = fopen("gui/results.json", "w");
    if (!f) return;
    fprintf(f, "{ \"N\": %d, \"matrix\": [", N);
    for (int i=0;i<N;i++) {
        fprintf(f, "[");
        for (int j=0;j<N;j++) {
            fprintf(f, "%.6f", C[i*N + j]);
            if (j+1<N) fprintf(f, ",");
        }
        fprintf(f, "]");
        if (i+1<N) fprintf(f, ",");
    }
    fprintf(f, "] }\n");
    fclose(f);
}

int main(int argc, char** argv) {
    signal(SIGINT, on_sigint);

    if (argc >= 2) N = atoi(argv[1]);
    if (argc >= 3) WORKERS = atoi(argv[2]);
    if (N <= 0) N = 200;
    if (WORKERS <= 0) WORKERS = 4;

    cout << "Starting parallel matrix multiply: N="<<N<<", workers="<<WORKERS<<"\n";

    // keys
    key_t key_msg = ftok(".", 65);
    key_t key_shm = ftok(".", 66);
    key_t key_sem = ftok(".", 67);

    // create msg queue
    msqid = msgget(key_msg, IPC_CREAT | 0666);
    if (msqid == -1) { perror("msgget"); cleanup_ipc(); return 1; }

    // create shared memory for 3 matrices (A,B,C)
    size_t total_elems = (size_t)N * N * 3;
    shmid = shmget(key_shm, total_elems * sizeof(double), IPC_CREAT | 0666);
    if (shmid == -1) { perror("shmget"); cleanup_ipc(); return 1; }
    void* shmaddr = shmat(shmid, nullptr, 0);
    if (shmaddr == (void*)-1) { perror("shmat"); cleanup_ipc(); return 1; }
    double* shm = (double*)shmaddr;
    double* A = shm;
    double* B = shm + N*N;
    double* C = shm + 2*N*N;

    // Initialize matrices (deterministic)
    srand(123456);
    for (size_t i=0;i<(size_t)N*N;i++) {
        A[i] = (rand() % 10);
        B[i] = (rand() % 10);
        C[i] = 0.0;
    }

    // semaphores: one per row (binary)
    semid = semget(key_sem, N, IPC_CREAT | 0666);
    if (semid == -1) { perror("semget"); cleanup_ipc(); return 1; }
    unsigned short *initvals = (unsigned short*)malloc(N * sizeof(unsigned short));
    for (int i=0;i<N;i++) initvals[i] = 1;
    semun su;
    su.array = initvals;
    if (semctl(semid, 0, SETALL, su) == -1) { perror("semctl SETALL"); cleanup_ipc(); free(initvals); return 1; }
    free(initvals);

    // Create jobs: dynamic small chunks for better load balancing
    int chunk = max(1, N / (WORKERS * 6)); // smaller chunk -> better balance
    for (int r = 0; r < N; r += chunk) {
        jobbuf jb;
        jb.mtype = 1;
        jb.start_row = r;
        jb.end_row = min(N, r + chunk);
        if (msgsnd(msqid, &jb, sizeof(jobbuf)-sizeof(long), 0) == -1) {
            perror("msgsnd job");
        }
    }
    // sentinel messages to instruct workers to exit when no jobs
    for (int i=0;i<WORKERS;i++) {
        jobbuf jb;
        jb.mtype = 2;
        jb.start_row = -1;
        jb.end_row = -1;
        if (msgsnd(msqid, &jb, sizeof(jobbuf)-sizeof(long), 0) == -1) {
            perror("msgsnd sentinel");
        }
    }

    // Fork workers
    vector<pid_t> workers_pids;
    for (int w=0; w<WORKERS; ++w) {
        pid_t pid = fork();
        if (pid == 0) {
            // worker process
            while (true) {
                jobbuf jb;
                if (msgrcv(msqid, &jb, sizeof(jobbuf)-sizeof(long), 0, 0) == -1) {
                    perror("worker msgrcv");
                    exit(1);
                }
                if (jb.mtype == 2) {
                    // exit sentinel
                    exit(0);
                }
                int rs = jb.start_row, re = jb.end_row;
                struct sembuf sop;
                for (int row = rs; row < re; ++row) {
                    // P(row)
                    sop.sem_num = row;
                    sop.sem_op = -1;
                    sop.sem_flg = 0;
                    if (semop(semid, &sop, 1) == -1) {
                        perror("semop P");
                        // continue to avoid deadlock; but in production you'd handle
                    }

                    // compute row
                    for (int col = 0; col < N; ++col) {
                        double s = 0.0;
                        for (int k = 0; k < N; ++k) s += A[row*N + k] * B[k*N + col];
                        C[row*N + col] = s;
                    }

                    // V(row)
                    sop.sem_op = 1;
                    if (semop(semid, &sop, 1) == -1) {
                        perror("semop V");
                    }

                    // send progress to master (1 row completed)
                    pmsg pm;
                    pm.mtype = 3;
                    pm.delta = 1;
                    // best-effort: ignore send errors
                    msgsnd(msqid, &pm, sizeof(pmsg)-sizeof(long), 0);
                }
            }
            // never reached
            exit(0);
        } else if (pid > 0) {
            workers_pids.push_back(pid);
        } else {
            perror("fork");
        }
    }

    // Master: monitor progress messages and child exits
    int rows_completed = 0;
    write_progress_json(rows_completed, N);

    int live_workers = workers_pids.size();
    while (live_workers > 0) {
        // try to receive progress messages (non-blocking)
        pmsg pm;
        ssize_t r = msgrcv(msqid, &pm, sizeof(pmsg)-sizeof(long), 3, IPC_NOWAIT);
        while (r != -1) {
            if (pm.mtype == 3) {
                rows_completed += pm.delta;
                if (rows_completed > N) rows_completed = N;
                write_progress_json(rows_completed, N);
            }
            r = msgrcv(msqid, &pm, sizeof(pmsg)-sizeof(long), 3, IPC_NOWAIT);
        }
        // check for child exit
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        while (pid > 0) {
            live_workers--;
            pid = waitpid(-1, &status, WNOHANG);
        }
        // sleep a bit
        usleep(100000); // 100ms
    }

    // final progress
    write_progress_json(N, N);

    // write results
    write_results_json(C, N);

    // detach and cleanup
    shmdt(shmaddr);
    cleanup_ipc();
    cout << "Done. Results written to gui/results.json\n";
    return 0;
}
