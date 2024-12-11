#include <iostream>
#include <windows.h>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>


HANDLE readerSemaphore;
HANDLE writerMutex;   
HANDLE readerMutex; 
int activeReaders = 0;
int pendingWriters = 0;
int sharedResource = 0;
CRITICAL_SECTION accessLock;


std::mutex statsLock;
int readSuccessCount = 0;
int writeSuccessCount = 0;
int readFailCount = 0;
int writeFailCount = 0;
double totalReadDuration = 0.0;
double totalWriteDuration = 0.0;
double totalReadWaitTime = 0.0;
double totalWriteWaitTime = 0.0;

std::atomic<bool> keepRunning(true);

void simulateReader(int id, int delay) {
    while (keepRunning) {
        auto waitStartTime = std::chrono::high_resolution_clock::now();
        WaitForSingleObject(readerMutex, INFINITE); 

        EnterCriticalSection(&accessLock);
        if (pendingWriters > 0) {
            LeaveCriticalSection(&accessLock);
            ReleaseMutex(readerMutex);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            {
                std::lock_guard<std::mutex> lock(statsLock);
                readFailCount++;
                std::cout << "Reader " << id << " was blocked." << std::endl;
            }
            continue;
        }
        activeReaders++;
        LeaveCriticalSection(&accessLock);
        ReleaseMutex(readerMutex);

        auto waitEndTime = std::chrono::high_resolution_clock::now();
        totalReadWaitTime += std::chrono::duration<double>(waitEndTime - waitStartTime).count();

       
        auto readStartTime = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        std::cout << "Reader " << id << " read value: " << sharedResource << std::endl;
        auto readEndTime = std::chrono::high_resolution_clock::now();

        EnterCriticalSection(&accessLock);
        if (--activeReaders == 0) {
            ReleaseMutex(writerMutex);
        }
        LeaveCriticalSection(&accessLock);

        {
            std::lock_guard<std::mutex> lock(statsLock);
            readSuccessCount++;
            totalReadDuration += std::chrono::duration<double>(readEndTime - readStartTime).count();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void simulateWriter(int id, int delay) {
    while (keepRunning) {
        auto waitStartTime = std::chrono::high_resolution_clock::now();
        ++pendingWriters;
        DWORD waitResult = WaitForSingleObject(writerMutex, 10);
        if (waitResult == WAIT_TIMEOUT) {
            std::lock_guard<std::mutex> lock(statsLock);
            writeFailCount++;
            --pendingWriters;
            std::cout << "Writer " << id << " timed out." << std::endl;
            continue;
        }
        auto waitEndTime = std::chrono::high_resolution_clock::now();
        totalWriteWaitTime += std::chrono::duration<double>(waitEndTime - waitStartTime).count();

        sharedResource++;
        std::cout << "Writer " << id << " wrote value: " << sharedResource << std::endl;
        ReleaseMutex(writerMutex);
        --pendingWriters;

        {
            std::lock_guard<std::mutex> lock(statsLock);
            writeSuccessCount++;
            totalWriteDuration += delay / 1000.0;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void releaseResources() {
    CloseHandle(readerSemaphore);
    CloseHandle(writerMutex);
    CloseHandle(readerMutex); 
    DeleteCriticalSection(&accessLock);
}

int main() {

    int readerCount = 2;
    int writerCount = 3;
    int readerDelay = 250;
    int writerDelay = 1500;

    readerSemaphore = CreateSemaphore(NULL, readerCount, readerCount, NULL);
    writerMutex = CreateMutex(NULL, FALSE, NULL);
    readerMutex = CreateMutex(NULL, FALSE, NULL);
    InitializeCriticalSection(&accessLock);

    std::vector<std::thread> threads;
    for (int i = 0; i < readerCount; ++i) {
        threads.emplace_back(simulateReader, i + 1, readerDelay);
    }

    for (int i = 0; i < writerCount; ++i) {
        threads.emplace_back(simulateWriter, i + 1, writerDelay);
    }

    std::this_thread::sleep_for(std::chrono::seconds(5));
    keepRunning = false;
    for (auto& thread : threads) {
        thread.join();
    }

    
    std::cout << std::endl <<"Total   Successful Reads: " << readSuccessCount << "    Failed Reads : " << readFailCount << std::endl;
    std::cout << "Total   Successful Writes: " << writeSuccessCount << "    Failed Writes: "<< writeFailCount << std::endl;
    std::cout << "Average Read Time: " << (readSuccessCount ? totalReadDuration / readSuccessCount : 0) << " s";
    std::cout << "    Write Time: " << (writeSuccessCount ? totalWriteDuration / writeSuccessCount : 0) << " s" << std::endl;
    std::cout << "Total   Reader Wait Time: " << totalReadWaitTime << " s";
    std::cout << "    Writer Wait Time: " << totalWriteWaitTime << " s" << std::endl;

    releaseResources();
    return 0;
}
