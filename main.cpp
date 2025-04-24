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

const size_t MAX_BUFFER_SIZE = 5;
std::queue<Process> processQueue;
std::mutex mtx;
std::condition_variable cv;
bool doneProducing = false;


void simulateProcess(const Process& p) {
    std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime));
    std::cout << "Process " << p.pid << " started (Priority: " << p.priority << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
    std::cout << "Process " << p.pid << " finished." << std::endl;
}

void producer(const std::string& filename) {
    std::ifstream infile(filename);
    std::string line;
    if (!infile) {
        std::cerr << "Failed to open processes.txt" << std::endl;
        std::lock_guard<std::mutex> lock(mtx);
        doneProducing = true;
        cv.notify_all();
        return;
    }

    std::getline(infile, line);

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        Process p;
        if (!(iss >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority)) {
            continue;
        }

        std::unique_lock<std::mutex> lock(mtx); 
        cv.wait(lock, [] {
            return processQueue.size() < MAX_BUFFER_SIZE;
        });

        processQueue.push(p);
        // Add output here for producer

        cv.notify_all();
    }

    std::lock_guard<std::mutex> lock(mtx);
    doneProducing = true;
    cv.notify_all();
}


void consumer (int id) {
    while (true) {
        Process p;
        { // Creats block scope
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [] {
                return !processQueue.empty() || doneProducing;
            });

            if (processQueue.empty() && doneProducing) {
                return;
            }

            p = processQueue.front();
            processQueue.pop();
            cv.notify_all();
        }

        std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime));
        std::cout << "[Consumer " << id << "] Started Process " << p.pid << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
        std::cout << "[Consumer " << id << "] Finished Process " << p.pid << "\n";
    }
}

int main() {
    const int NUM_CONSUMERS = 3;

    std::thread prodThread(producer, "processes.txt");

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

