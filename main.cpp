#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>

int NUMBER_OF_THREADS = 4;

using namespace std;

void Rec_Mult(double *C, double *A, double *B, int n, int row) {

    if (n == 1) {
        C[0] += A[0] * B[0];
    }
    else {
        int d11 = 0;
        int d12 = n / 2;
        int d21 = (n / 2) * row;
        int d22 = (n / 2) * (row + 1);

        // C11 += A11 * B11
        Rec_Mult(C + d11, A + d11, B + d11, n / 2, row);
        // C11 += A12 * B21
        Rec_Mult(C + d11, A + d12, B + d21, n / 2, row);

        // C12 += A11 * B12
        Rec_Mult(C + d12, A + d11, B + d12, n / 2, row);
        // C12 += A12 * B22
        Rec_Mult(C + d12, A + d12, B + d22, n / 2, row);

        // C21 += A21 * B11
        Rec_Mult(C + d21, A + d21, B + d11, n / 2, row);
        // C21 += A22 * B21
        Rec_Mult(C + d21, A + d22, B + d21, n / 2, row);

        // C22 += A21 * B12
        Rec_Mult(C + d22, A + d21, B + d12, n / 2, row);
        // C22 += A22 * B22
        Rec_Mult(C + d22, A + d22, B + d22, n / 2, row);
    }
}

void printMatrix(double *D, int rowSize, int n, FILE *output) {
    for (int i = 0; i < n / rowSize; i++) {
        for (int j = 0; j < rowSize; j++) {
            int temp = 0;
            temp = (int) D[i * rowSize + j];
            fprintf(output, "%d ", temp);
        }
        fprintf(output, "\n");
    }
    fprintf(output, "\n");
}

void readMatrix(FILE *input, double *matrix, int size) {
    for (int i = 0; i < size; i++) {
        int temp;
        fscanf(input, "%d", &temp);
        matrix[i] = temp;
    }
}

void parseFile(FILE *input, double *A, double *B, int *size) {
    readMatrix(input, A, *size);
    readMatrix(input, B, *size);
}

void handler(int *readFds, int *writeFds, int *controlFds) {
    int dist, size, rowSize;
    close(readFds[1]);
    close(writeFds[0]);
    close(controlFds[0]);

    write(controlFds[1], "ready\0", sizeof(char) * 6);

    read(readFds[0], &dist, sizeof(int));
    read(readFds[0], &size, sizeof(int));
    read(readFds[0], &rowSize, sizeof(int));
    double A[size], B[size], C[size];
    read(readFds[0], A, sizeof(double) * size);
    read(readFds[0], B, sizeof(double) * size);
    for (int i = 0; i < size; i++) {
        C[i] = 0;
    }
    int d11 = 0;
    int d12 = rowSize / 2;
    int d21 = (rowSize / 2) * rowSize;
    int d22 = (rowSize / 2) * (rowSize + 1);
    switch (dist) {
        case 1: {
            Rec_Mult(C + d11, A + d11, B + d11, rowSize / 2, rowSize);
            Rec_Mult(C + d11, A + d12, B + d21, rowSize / 2, rowSize);
            break;
        }
        case 2: {
            Rec_Mult(C + d12, A + d11, B + d12, rowSize / 2, rowSize);
            Rec_Mult(C + d12, A + d12, B + d22, rowSize / 2, rowSize);
            break;
        }
        case 3: {
            Rec_Mult(C + d21, A + d21, B + d11, rowSize / 2, rowSize);
            Rec_Mult(C + d21, A + d22, B + d21, rowSize / 2, rowSize);
            break;
        }
        case 4: {
            Rec_Mult(C + d22, A + d21, B + d12, rowSize / 2, rowSize);
            Rec_Mult(C + d22, A + d22, B + d22, rowSize / 2, rowSize);
            break;
        }
        default:
            break;
    }
    write(writeFds[1], &rowSize, sizeof(int));
    write(writeFds[1], &size, sizeof(int));
    write(writeFds[1], &dist, sizeof(int));
    write(writeFds[1], C, sizeof(double) * size);
    exit(0);
}

int select(int *fds) {
    char buf[5];
    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        fcntl(fds[i * 2], F_SETFL, O_NONBLOCK);
        read(fds[i * 2], buf, sizeof(char) * 6);
        if (strcmp(buf, "ready\0") == 0) {
            return i;
        }
    }
    return -1;
}

void sendData(int number, int fds, double *A, double *B, int size, int rowSize) {
    number++;
    write(fds, &number, sizeof(int));
    write(fds, &size, sizeof(int));
    write(fds, &rowSize, sizeof(int));
    write(fds, A, sizeof(double) * size);
    write(fds, B, sizeof(double) * size);
}

void src(int *fds, int *controlFds, char *path) {
    FILE *input = fopen(path, "r");
    int size, rowSize;
    fscanf(input, "%d %d", &rowSize, &size);
    double A[size], B[size];
    parseFile(input, A, B, &size);
    int count = 0;
    while (count != 4) {
        int channel = select(controlFds);
        if (channel != -1) {
            sendData(count, fds[channel * 2 + 1], A, B, size, rowSize);
            count++;
        }
        else{
            sleep(3);
        }

    }
}

void assemblyResult(int *fds, double *C, int *size, int *rowSize, FILE *output) {
    int dist;
    for (int i = 0; i < 4; i++) {
        read(fds[i * 2], &(*rowSize), sizeof(int));
        read(fds[i * 2], &(*size), sizeof(int));
        read(fds[i * 2], &dist, sizeof(int));
        double temp[*size];
        read(fds[i * 2], temp, sizeof(double) * *size);
        if (i == 0) {
            C = (double *) malloc((size_t) sizeof(double) * *size);
        }
        for (int j = 0; j < *size; j++) {
            C[j] += temp[j];
        }
    }
    printMatrix(C, *rowSize, *size, output);
}

void dst(int *fds,char *path) {
    FILE *output = fopen(path, "w");
    int size, rowSize;
    double *C;
    assemblyResult(fds, C, &size, &rowSize, output);
}

void initThread(int number, int *readFds, int *writeFds, int *controlFds) {
    int writeFildes[2];
    int readFildes[2];
    int controlFildes[2];
    if (pipe(writeFildes) == -1 || pipe(readFildes) == -1 || pipe(controlFildes) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }
    writeFds[number * 2] = writeFildes[0];
    writeFds[number * 2 + 1] = writeFildes[1];
    readFds[number * 2] = readFildes[0];
    readFds[number * 2 + 1] = readFildes[1];
    controlFds[number * 2] = controlFildes[0];
    controlFds[number * 2 + 1] = controlFildes[1];

    switch (fork()) {
        case -1:
            perror("fork");
            exit(1);
        case 0: {
            handler(readFildes, writeFildes, controlFildes);
            exit(0);
        }
        default:
            break;
    }
}

int main(int argc, char **argv) {

    int readFds[NUMBER_OF_THREADS * 2];
    int writeFds[NUMBER_OF_THREADS * 2];
    int controlFds[NUMBER_OF_THREADS * 2];

    for (int i = 0; i < NUMBER_OF_THREADS; i++) {
        initThread(i, readFds, writeFds, controlFds);
    }
    switch (fork()) {
        case -1:
            perror("error");
            exit(1);
        case 0: {
            dst(writeFds, argv[2]);
            return 0;
        }
        default : {
            int status;
            src(readFds, controlFds, argv[1]);
            while (errno != ECHILD) {
                wait(&status);
            }
        }
    }
    return 0;
}