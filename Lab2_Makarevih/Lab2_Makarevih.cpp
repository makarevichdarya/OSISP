#include <windows.h>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <vector>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <numeric>
#include <algorithm>
#define BASE_BUFFER_SIZE 256 // Базовый размер буфера для асинхронных операций

using namespace std;

DWORD asyncIterationCount = 0; // Счётчик итераций асинхронного чтения
DWORD syncIterationCount = 0;  // Счётчик итераций синхронного чтения
int asyncReadCount = 0;        // Счётчик операций асинхронного чтения

// Переменные для замера времени выполнения различных этапов
std::chrono::duration<double> asyncReadDuration;
std::chrono::duration<double> processingDuration;
std::chrono::duration<double> asyncWriteDuration;

// Функция для вычисления статистики: среднее, стандартное отклонение, минимум и максимум
void computeStatistics(const std::vector<int>& data, double& mean, double& stdev, int& minValue, int& maxValue) {
    int size = data.size();
    if (size == 0) return; // Если данных нет, выходим из функции

    // Вычисление среднего значения
    mean = std::accumulate(data.begin(), data.end(), 0.0) / size;

    // Нахождение минимального и максимального значений
    minValue = *std::min_element(data.begin(), data.end());
    maxValue = *std::max_element(data.begin(), data.end());

    // Вычисление стандартного отклонения
    double varianceSum = 0.0;
    for (int value : data) {
        varianceSum += (value - mean) * (value - mean);
    }
    stdev = std::sqrt(varianceSum / size);
}

// Функция для создания тестового файла с данными
void createDataFile(const std::wstring& filename, int count) {
    std::ofstream outFile(filename);
    if (outFile.is_open()) {
        for (int i = 0; i < count; i++) {
            outFile << count - i << " "; // Запись чисел от count до 1
        }
        outFile.close();
    }
    else {
        std::wcout << L"Error opening file for writing: " << filename << "\n";
    }
}

// Функция для сохранения статистики в файл
void saveToFile(double mean, double stdev, int minValue, int maxValue, const std::wstring& filename) {
    HANDLE hFile = CreateFile(
        filename.c_str(),
        GENERIC_WRITE,
        FILE_APPEND_DATA,
        NULL,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD writtenBytes;
        // Форматируем строку со статистикой
        std::wstring result = L"Mean: " + std::to_wstring(mean) + L", Std Dev: " + std::to_wstring(stdev) +
            L", Min: " + std::to_wstring(minValue) + L", Max: " + std::to_wstring(maxValue) + L"\n";
        // Запись строки в файл
        WriteFile(hFile, result.c_str(), result.size() * sizeof(wchar_t), &writtenBytes, NULL);
        CloseHandle(hFile);
    }
    else {
        std::wcerr << L"Error opening file for writing: " << filename << '\n';
    }
}

// Callback-функция, вызываемая после завершения асинхронного чтения
void CALLBACK onAsyncReadComplete(DWORD errorCode, DWORD bytesRead, LPOVERLAPPED lpOverlapped) {
    if (errorCode == 0) {
        // Обновляем смещение в структуре OVERLAPPED
        lpOverlapped->Offset += bytesRead;
        char* buffer = (char*)lpOverlapped->hEvent;
        buffer[bytesRead] = '\0';

        // Преобразуем данные из буфера в вектор целых чисел
        std::vector<int> data;
        std::istringstream iss(buffer);
        int value;
        while (iss >> value) {
            data.push_back(value);
        }

        // Измеряем время обработки данных
        auto processingStart = std::chrono::high_resolution_clock::now();

        // Вычисляем статистику
        double mean, stdev;
        int minValue, maxValue;
        computeStatistics(data, mean, stdev, minValue, maxValue);

        auto processingEnd = std::chrono::high_resolution_clock::now();
        processingDuration = processingEnd - processingStart;

        // Сохраняем статистику в файл и замеряем время записи
        processingStart = std::chrono::high_resolution_clock::now();
        saveToFile(mean, stdev, minValue, maxValue, L"stats_async.txt");
        processingEnd = std::chrono::high_resolution_clock::now();
        asyncWriteDuration = processingEnd - processingStart;
    }
    else {
        if (errorCode == 38) {
            return; // Код ошибки 38 означает конец файла
        }
        std::cerr << "Error reading file: " << errorCode << std::endl;
    }
}

// Выполнение асинхронного чтения данных из файла
void performAsyncFileOperations(int bufferSize) {
    HANDLE hFile = CreateFile(L"data.txt", GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Error opening file: " << GetLastError() << std::endl;
        return;
    }

    // Выделяем память для буфера и настраиваем структуру OVERLAPPED
    char* buffer = new char[bufferSize];
    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = (HANDLE)buffer;

    // Очищаем файл с асинхронными результатами
    std::ofstream outFile(L"stats_async.txt");
    outFile.close();

    while (true) {
        DWORD previousOffset = overlapped.Offset;
        // Запуск асинхронного чтения
        if (!ReadFileEx(hFile, buffer, bufferSize - 1, &overlapped, onAsyncReadComplete)) {
            std::cerr << "Error initiating read: " << GetLastError() << std::endl;
            CloseHandle(hFile);
            return;
        }
        asyncReadCount++;

        // Ожидание завершения чтения с вызовом callback
        SleepEx(INFINITE, TRUE);

        // Проверяем, достигнут ли конец файла
        auto fileSize = GetFileSize(hFile, NULL);
        if (fileSize <= overlapped.Offset) {
            break;
        }
        if (previousOffset == overlapped.Offset) {
            break;
        }
    }

    CloseHandle(hFile);
    delete[] buffer;
}

// Выполнение синхронного чтения данных из файла
DWORD performSyncFileOperations() {
    HANDLE hFile = CreateFile(
        L"data.txt",
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Could not open file (Error: " << GetLastError() << ").\n";
        return -1;
    }

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == INVALID_FILE_SIZE) {
        std::cerr << "Could not get file size (Error: " << GetLastError() << ").\n";
        CloseHandle(hFile);
        return -1;
    }

    // Чтение данных в буфер
    char* buffer = new char[fileSize + 1];
    if (!buffer) {
        std::cerr << "Memory allocation failed." << std::endl;
        CloseHandle(hFile);
        return -1;
    }

    DWORD bytesRead;
    if (!ReadFile(hFile, buffer, fileSize, &bytesRead, NULL)) {
        std::cerr << "Could not read file (Error: " << GetLastError() << ")." << std::endl;
        delete[] buffer;
        CloseHandle(hFile);
        return -1;
    }

    buffer[bytesRead] = '\0';

    // Преобразуем данные из буфера в вектор целых чисел и вычисляем статистику
    std::vector<int> data;
    std::istringstream iss(buffer);
    int value;
    while (iss >> value) {
        data.push_back(value);
    }

    double mean, stdev;
    int minValue, maxValue;
    computeStatistics(data, mean, stdev, minValue, maxValue);

    // Сохраняем результаты статистики в файл
    saveToFile(mean, stdev, minValue, maxValue, L"stats_sync.txt");

    delete[] buffer;
    CloseHandle(hFile);
    return fileSize + 1;
}

int main() {
    // Создаём текстовый файл с 10000 чисел
    createDataFile(L"data.txt", 10000);

    int i = 0;
    std::cout << "Asynchronous Execution:\n";
    while (true) {
        int bufferMultiplier = pow(2, i);
        i++;
        int maxBufferSize = BASE_BUFFER_SIZE * bufferMultiplier;
        if (maxBufferSize >= 1e5) {
            break; // Останавливаемся, если буфер стал больше 100000 байт
        }

        // Измеряем общее время выполнения асинхронного чтения
        auto asyncStart = std::chrono::high_resolution_clock::now();
        performAsyncFileOperations(maxBufferSize);
        auto asyncEnd = std::chrono::high_resolution_clock::now();

        // Рассчитываем время работы и выводим результаты
        std::chrono::duration<double> asyncDuration = asyncEnd - asyncStart;
        std::cout << "Buffer size: " << maxBufferSize << '\t'
            << fixed << setprecision(3) << " Total time: " << asyncDuration.count() << '\t'
            << " Write time: " << asyncWriteDuration.count() * asyncReadCount << "\t"
            << " Read time: " << abs(asyncDuration.count() - (asyncWriteDuration.count() + processingDuration.count()) * asyncReadCount) << "\t"
            << " Processing time: " << processingDuration.count() * asyncReadCount << "\n";

        // Сбрасываем счётчики для следующей итерации
        asyncIterationCount = 0;
        asyncReadCount = 0;
    }

    // Измеряем время выполнения синхронного чтения
    auto syncStart = std::chrono::high_resolution_clock::now();
    auto syncBufferSize = performSyncFileOperations();
    auto syncEnd = std::chrono::high_resolution_clock::now();

    // Выводим результаты синхронного выполнения
    std::chrono::duration<double> syncDuration = syncEnd - syncStart;
    std::cout << "\nSynchronous Execution:\n";
    std::cout << "Buffer size: " << syncBufferSize << '\t'
        << " Total time: " << syncDuration.count() << '\n';

    return 0;
}
