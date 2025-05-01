#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>
#include <queue>
#include <mutex>
#include <condition_variable>

//struct to hold process information
struct Process {
    int pid;
    int arrivalTime;
    int burstTime;
    int priority;
};

// Global Variables
static std::chrono::time_point<std::chrono::steady_clock> programStart;
const size_t MAX_BUFFER_SIZE = 5;
std::queue<Process> processQueue; //Shared queue for processes
std::mutex mtx; // For queue synchronization
std::mutex cout_mtx; // For output synchronization
std::condition_variable cv_producer, cv_consumer;
bool producerDone = false; //Flag indicating if producer is finished


// Prints Consumer # message
void printConsumerProcess(int consumer_id, const Process& p, const std::string& event, long elapsed) {
    std::lock_guard<std::mutex> cout_lock(cout_mtx);
    std::cout << "[Consumer " << consumer_id << "] Process " << p.pid 
              << " " << event << " (Priority: " << p.priority << ") (t=" 
              << elapsed << ")" <<std::endl;
}

//Simulate the execution of a process 
void simulateProcess(const Process& p) {
    // Calculate how much time has elapsed since start of program
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
    
    // If the process hasn't arrived, sleep until it does
    if (p.arrivalTime > elapsed) {
        std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime - elapsed));
        elapsed = p.arrivalTime;
    }
    
    std::cout << "Process " << p.pid << " started (Priority: " << p.priority << ") (t=" << elapsed << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
    
    // Calculates end time
    now = std::chrono::steady_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
    std::cout << "Process " << p.pid << " finished (Priority: " << p.priority << ") (t=" << elapsed << ")" << std::endl;
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
        cv_consumer.notify_all(); // Notify consumer to ublock
        return;
    }

    std::getline(infile, line); // Skip Header

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        Process p;
        if (!(iss >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority)) {
            continue; //skip malformed lines
        }

        std::unique_lock<std::mutex> lock(mtx);
        cv_producer.wait(lock, [] { // Wait until there is space in the buffer
            return processQueue.size() < MAX_BUFFER_SIZE;
        });

        processQueue.push(p); // Add Process to queue
        cv_consumer.notify_all(); //notify waiting consumers
    }

    std::lock_guard<std::mutex> lock(mtx);
    producerDone = true;
    cv_consumer.notify_all(); //notify consumers to finish
}

//consumer function to process items from the queue
void consumer (int id) {
    while (true) {
        Process p;
        { // Creates block scope for threads
            std::unique_lock<std::mutex> lock(mtx);
            // Wait until the queue is not empty or production is done
            cv_consumer.wait(lock, [] {
                return !processQueue.empty() || producerDone;
            });

            // Exits thread
            if (processQueue.empty() && producerDone) {
                return;
            }

            p = processQueue.front(); // Holds data
            processQueue.pop();
            cv_producer.notify_all(); // Notify producer that space is available
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
        
        // If the process hasn't arrived, sleep until it does
        if (p.arrivalTime > elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime - elapsed));
            elapsed = p.arrivalTime;
        }
        
        // Print Consumer statements
        printConsumerProcess(id, p, "started", elapsed);
        
        std::this_thread::sleep_for(std::chrono::seconds(p.burstTime)); //simulate work
        
        now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
        printConsumerProcess(id, p, "finished", elapsed);
    }
}


// Printing thread activity logging
void logActivity(int id, const std::string& action) {
    std::lock_guard<std::mutex> cout_lock(cout_mtx);
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
    std::cout << "[Consumer " << id << "] " << action << " (t=" << elapsed << ")" << std::endl;
}

void runThreadACtivityLogging() {
    const int NUM_CONSUMERS = 3;
    programStart = std::chrono::steady_clock::now(); // Resets the timer
    producerDone = false; // Resets the flag
    processQueue = {}; // Resets/clears the queue
    
    // Defines logging consumer
    auto logging_consumer = [](int id) {
        logActivity(id, "Starting thread");
        
        while (true) {
            Process p;
            {
                logActivity(id, "Waiting for work");
                std::unique_lock<std::mutex> lock(mtx);
                
                cv_consumer.wait(lock, [] {
                    return !processQueue.empty() || producerDone;
                });
                
                if (processQueue.empty() && producerDone) {
                    logActivity(id, "Exiting thread");
                    return;
                }
                
                logActivity(id, "Acquired item from queue");
                p = processQueue.front();
                processQueue.pop();
                cv_producer.notify_all();
                logActivity(id, "Notified producer");
            }
            
            // Execute processes with logging_consumer
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
        
            if (p.arrivalTime > elapsed) {
                logActivity(id, "Waiting for process arrival time");
                std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime - elapsed));
                elapsed = p.arrivalTime;
            }
            
            logActivity(id, "Starting process execution");
            printConsumerProcess(id, p, "started", elapsed);
            
            std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
            
            now = std::chrono::steady_clock::now();
            elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - programStart).count();
        
            logActivity(id, "Completed process execution");
            printConsumerProcess(id, p, "finished", elapsed);
        }
    };
    
    std::thread prodThread(producer, "processes.txt"); // Start producer thread
    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(logging_consumer, i + 1);
    }
    
    prodThread.join();
    for (auto& t : consumers) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    std::cout << "All processes have completed.\n" << std::endl;
}


int main() {
    // ------------------------------------------------------
    // ----- 1. Simulating Processes with Threads -----------
    // ------------------------------------------------------
    std::cout << "1. Simulating processes with threads:" << std::endl;
    {
        std::ifstream infile("processes.txt");
        std::string line;
        std::vector<Process> processList;
        std::vector<std::thread> threads;
    
        if (!infile) {
            std::cerr << "Failed to open processes.txt" << std::endl;
            return 1;
        }
    
        std::getline(infile, line); //skip header
    
        while (std::getline(infile, line)) {
            std::istringstream iss(line);
            Process p;
            if (iss >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority) {
                processList.push_back(p);
            }
        }
        
        // Initialize global start time
        programStart = std::chrono::steady_clock::now();
        
        for (const auto& p : processList) {
            threads.emplace_back(simulateProcess, p);
        }
    
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        
        std::cout << "All processes have completed.\n" << std::endl;
    }
    
    // ------------------------------------------------------
    // ----- 2. Adding Producer-Consumer Implementation ----- 
    // ------------------------------------------------------
    std::cout << "2. Producer-Consumer Implementation:" << std::endl;
    {
        const int NUM_CONSUMERS = 3;
        programStart = std::chrono::steady_clock::now(); // Resets the timer
        producerDone = false; // Resets the flag
        processQueue = {}; // Resets/clears the queue
        
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
    
        std::cout << "All processes have completed.\n" << std::endl;
    }
    
    // ------------------------------------------------------
    // ----- 3. Showing Thread Activity ---------------------
    // ------------------------------------------------------
    std::cout << "3. Showing Thread Activity:" << std::endl;
    runThreadACtivityLogging();
    
    return 0;
}
