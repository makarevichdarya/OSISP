#include <iostream>
#include <windows.h>
#include <chrono>
#include <fstream>

// Перемножение матриц - функция, которую выполняет поток
DWORD WINAPI FunctionByThread(LPVOID lpParam)
{
    auto start = std::chrono::high_resolution_clock::now();
    DWORD ThreadId = GetCurrentThreadId();
    int iteration = 0;
    int A[4][4] = {
        {135, 2344324, 3312312, 4312312},
        {5312312, 3123126, 3123127, 3545348},
        {5345349, 15345340, 1543531, 12534534},
        {15345343, 15345344, 15543534, 16534534}
    };
    int B[4][4] = {
        {16, 15, 14, 13},
        {12, 11, 10, 9},
        {8, 7, 6, 5},
        {4, 3, 2, 1}
    };
    int C[4][4] = { 0 };

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            C[i][j] = 0;
            for (int k = 0; k < 4; k++) {
                C[i][j] += A[i][k] * B[k][j];
                iteration++;
                // Добавляем дополнительную вычислительную нагрузку на поток
                for (int l = 0; l < 1000000; l++)
                {
                    for (int o = 0; o < 300; o++)
                        volatile int temp = o * l;
                }

                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double> elapsed = end - start;
                std::cout << "Iteration " << iteration << " of thread. ID: " << ThreadId << ". Time: " << elapsed.count() << " seconds" << std::endl;
            }
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Thread ID: " << ThreadId << ". Finished after " << elapsed.count() << " seconds. Performance: " << iteration / elapsed.count() << std::endl;
    std::ofstream logFile("thread_log.log", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << "Thread " << ThreadId << ". Priority: " << GetThreadPriority(GetCurrentThread())
            << ". Finished after " << elapsed.count() << " seconds. Performance is " << iteration / elapsed.count() << ".\n";
        logFile.close();
    }
    else {
        std::cerr << "Error opening file for writing\n";
    }
    return 0;
}
int main()
{
    const int priorities[] = {
        THREAD_PRIORITY_LOWEST,
        THREAD_PRIORITY_BELOW_NORMAL,
        THREAD_PRIORITY_NORMAL,
        THREAD_PRIORITY_ABOVE_NORMAL,
        THREAD_PRIORITY_HIGHEST
    };
    const int numPriorities = sizeof(priorities) / sizeof(priorities[0]);
    srand(static_cast<unsigned int>(time(NULL)));
    std::ofstream clearFile("thread_log.log", std::ios_base::trunc);
    clearFile.close();
    int randomPriority = 0;
    int number = -1;

    // Ввод количества потоков
    while (number <= 0 || number > 65) {
        std::cout << "Input number of threads 1-65 (64 is limit value of threads. 65 - treads are terminated after starting).\n";
        std::cin >> number;
    }

    // Создание массива потоков
    HANDLE* Threads = new HANDLE[number];
    for (int i = 0; i < number; i++)
    {
        Threads[i] = CreateThread(
            NULL,                            // Атрибут безопасности 
            0,                               // Размер стека 
            FunctionByThread,                // Выполняемая стеком функция
            0,                               // Передаваемый параметр
            CREATE_SUSPENDED,                // Флаг создания 
            0);                              // Указатель на переменную для ID потока
        if (Threads[i] == NULL)
        {
            std::cout << "Error in creating " << GetThreadId(Threads[i]) << " thread\n";
        }
    }
    // Выставление приоритетов для потоков случайным образом
    for (int k = 0; k < number; k++)
    {
        randomPriority = priorities[rand() % numPriorities];
        if (!SetThreadPriority(Threads[k], randomPriority))
        {
            std::cout << "Error in setting priority of " << GetThreadId(Threads[k]) << "thread\n";
        }
        std::cout << "Thread " << GetThreadId(Threads[k]) << " started. Priority: " << randomPriority << "\n";
    }
    // Возобновление потоков после выставления приоритетов
    for (int i = 0; i < number; i++)
    {
        if (!ResumeThread(Threads[i]))
        {
            std::cout << "Error in resuming thread with id " << GetThreadId(Threads[i]) << " \n";
        }
        std::cout << "Thread " << GetThreadId(Threads[i]) << " is running\n";
    }
    std::cout << "All threads are running\n";

    // Ожидание выполнения потоков
    WaitForMultipleObjects(number, Threads, TRUE, INFINITE);

    // Закрытие потоков
    for (int i = 0; i < number; ++i) {
        CloseHandle(Threads[i]);
    }
    std::cout << "All threads are finished." << std::endl;
}