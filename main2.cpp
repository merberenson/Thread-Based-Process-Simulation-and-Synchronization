#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>
#include <chrono>

struct Process {
    int pid;
    int arrivalTime;
    int burstTime;
    int priority;
};

void simulateProcess(const Process& p) {
    std::this_thread::sleep_for(std::chrono::seconds(p.arrivalTime));
    std::cout << "Process " << p.pid << " started (Priority: " << p.priority << ")" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(p.burstTime));
    std::cout << "Process " << p.pid << " finished." << std::endl;
}

int main() {
    std::ifstream infile("processes.txt");
    std::string line;
    std::vector<Process> processList;
    std::vector<std::thread> threads;

    if (!infile) {
        std::cerr << "Failed to open processes.txt" << std::endl;
        return 1;
    }

    std::getline(infile, line);

    while (std::getline(infile, line)) {
        std::istringstream iss(line);
        Process p;
        if (iss >> p.pid >> p.arrivalTime >> p.burstTime >> p.priority) {
            processList.push_back(p);
        }
    }

    for (const auto& p : processList) {
        threads.emplace_back(simulateProcess, p);
    }

    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::cout << "All processes have completed." << std::endl;
    return 0;
}

