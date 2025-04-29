#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>

struct Process {
    int pid;
    int arrivalTime;
    int burstTime;
    int priority;
};

// Global Variables
const size_t MAX_BUFFER_SIZE = 5;
std::queue<Process> processQueue; 
std::mutex mtx;
std::condition_variable cv;
bool producerDone = false;


void simulateProcess(const Process& p) {
    std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime));
    std::cout << "Process " << p.pid << " started (Priority: " << p.priority << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
    std::cout << "Process " << p.pid << " finished." << std::endl;
}

// Reads process from file and adds them to queue
void producer(const std::string& filename) {
    std::ifstream infile(filename);
    std::string line;

    // Ensures file can be opened
    if (!infile) {
        std::cerr << "Failed to open processes.txt" << std::endl;
        std::lock_guard<std::mutex> lock(mtx);
        producerDone = true;
        cv.notify_all(); // Notify consumer to ublock
        return;
    }

    std::getline(infile, line); // Skip Header

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        Process p;
        if (!(iss >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority)) {
            continue;
        }

        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [] { // Wait until there is space in the buffer
            return processQueue.size() < MAX_BUFFER_SIZE;
        });

        processQueue.push(p); // Add Process to queue
        cv.notify_all();
    }

    std::lock_guard<std::mutex> lock(mtx);
    producerDone = true;
    cv.notify_all();
}


void consumer (int id) {
    while (true) {
        Process p;
        { // Creats block scope for threads
            std::unique_lock<std::mutex> lock(mtx);
            // Wait until the queue is not empty or production is done
            cv.wait(lock, [] {
                return !processQueue.empty() || producerDone;
            });

            // Exits thread
            if (processQueue.empty() && producerDone) {
                return;
            }

            p = processQueue.front(); // Holds data
            processQueue.pop();
            cv.notify_all(); // Notify producer that space is available
        }
        // Simulate delay for arrival time
        std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime));
        std::cout << "[Consumer " << id << "] Started Process " << p.pid << std::endl;
        // Simulate CPU burst time
        std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
        std::cout << "[Consumer " << id << "] Finished Process " << p.pid << "\n";
    }
}

int main() {
    const int NUM_CONSUMERS = 3;

    std::thread prodThread(producer, "processes.txt"); // Start producer thread

    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(consumer, i + 1);
    }


    prodThread.join();
    for (auto& t : consumers) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::cout << "All processes have completed." << std::endl;
    return 0;
}

