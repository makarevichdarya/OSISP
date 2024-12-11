#include <windows.h>
#include <stdio.h>

#define BUFFER_SIZE 1024
#define PIPE_NAME L"\\\\.\\pipe\\LogPipe"

int main() {
    HANDLE pipe;
    char buffer[BUFFER_SIZE];
    DWORD bytesWritten;

    // Попытка подключиться к каналу с ожиданием
    while (1) {
        pipe = CreateFile(
            PIPE_NAME,
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (pipe != INVALID_HANDLE_VALUE) {
            printf("Successfully connected to the pipe.\n");
            break;
        }

        // Печать сообщения об ошибке только в первый раз
        if (GetLastError() != ERROR_PIPE_BUSY) {
            fprintf(stderr, "Failed to connect to pipe. Error: %ld\n", GetLastError());
            return 1;
        }

        printf("Pipe is busy, retrying in 1 second...\n");
        Sleep(1000);
    }

    // Цикл для отправки сообщений
    while (1) {
        printf("Enter message (or type 'exit' to quit): ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = '\0'; // Удаляем символ новой строки

        // Проверка на команду выхода
        if (strcmp(buffer, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        if (!WriteFile(pipe, buffer, (DWORD)strlen(buffer) + 1, &bytesWritten, NULL)) {
            fprintf(stderr, "Failed to write to pipe. Error: %ld\n", GetLastError());
            break;
        }

        printf("Message sent successfully.\n");
    }

    CloseHandle(pipe);
    return 0;
}
