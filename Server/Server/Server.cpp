#include <windows.h>
#include <stdio.h>
#include <time.h>

#define _CRT_SECURE_NO_WARNINGS 1
#define BUFFER_SIZE 1024
#define PIPE_NAME L"\\\\.\\pipe\\LogPipe"
#define MAX_CLIENTS 5

HANDLE logMutex;

void log_message(const char* client_id, const char* message) {
    WaitForSingleObject(logMutex, INFINITE);

    HANDLE file = CreateFile(
        L"server_log.txt",
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (file != INVALID_HANDLE_VALUE) {
        char log_entry[BUFFER_SIZE + 100];
        time_t now = time(NULL);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        char time_str[20];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);

        snprintf(log_entry, sizeof(log_entry), "[%s] Client %s: %s\n", time_str, client_id, message);

        DWORD bytesWritten;
        WriteFile(file, log_entry, (DWORD)strlen(log_entry), &bytesWritten, NULL);
        CloseHandle(file);
    }
    else {
        fprintf(stderr, "Failed to open log file. Error: %ld\n", GetLastError());
    }

    ReleaseMutex(logMutex);
}

DWORD WINAPI client_handler(LPVOID param) {
    HANDLE pipe = (HANDLE)param;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead;
    char client_id[10];
    snprintf(client_id, sizeof(client_id), "%d", GetCurrentThreadId());

    while (ReadFile(pipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        log_message(client_id, buffer);
    }

    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
    return 0;
}

int main() {
    HANDLE pipe;
    HANDLE threads[MAX_CLIENTS];
    int threadCount = 0;

    // Создаём (или очищаем) лог-файл
    HANDLE hFile = CreateFile(
        L"server_log.txt",
        GENERIC_WRITE,
        0,
        NULL,
        CREATE_ALWAYS,  // Изменено на CREATE_ALWAYS для очистки при запуске
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
    }

    logMutex = CreateMutex(NULL, FALSE, NULL);
    if (logMutex == NULL) {
        fprintf(stderr, "Failed to create mutex. Error: %ld\n", GetLastError());
        return 1;
    }

    while (1) {
        pipe = CreateNamedPipe(
            PIPE_NAME,
            PIPE_ACCESS_INBOUND,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            MAX_CLIENTS,
            BUFFER_SIZE,
            BUFFER_SIZE,
            0,
            NULL);

        if (pipe == INVALID_HANDLE_VALUE) {
            fprintf(stderr, "Failed to create named pipe. Error: %ld\n", GetLastError());
            return 1;
        }

        printf("Waiting for clients to connect...\n");

        BOOL connected = ConnectNamedPipe(pipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected) {
            printf("Client connected.\n");

            if (threadCount < MAX_CLIENTS) {
                threads[threadCount] = CreateThread(
                    NULL,
                    0,
                    client_handler,
                    (LPVOID)pipe,
                    0,
                    NULL
                );

                if (threads[threadCount] == NULL) {
                    fprintf(stderr, "Create Thread failed. Error: %ld\n", GetLastError());
                    CloseHandle(pipe);
                }
                else {
                    threadCount++;
                }
            }
            else {
                fprintf(stderr, "Max clients reached. Connection refused.\n");
                DisconnectNamedPipe(pipe);
                CloseHandle(pipe);
            }
        }
        else {
            CloseHandle(pipe);
        }
    }

    WaitForMultipleObjects(threadCount, threads, TRUE, INFINITE);

    for (int i = 0; i < threadCount; i++) {
        CloseHandle(threads[i]);
    }

    CloseHandle(logMutex);
    return 0;
}