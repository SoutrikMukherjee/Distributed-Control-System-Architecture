#pragma once

#include "module.h"
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <dlfcn.h>

namespace dcs {

// Configuration structure
struct Config {
    size_t sharedMemorySize{100 * 1024 * 1024}; // 100MB default
    size_t messageQueueSize{10000};
    bool enableRedundancy{false};
    bool enableMetrics{true};
    std::string logLevel{"INFO"};
    std::chrono::milliseconds watchdogTimeout{5000};
};

// Control loop definition
struct ControlLoop {
    std::string name;
    double frequency;
    std::vector<std::string> sensorModules;
    std::vector<std::string> actuatorModules;
    ActuatorCallback controlFunction;
    std::thread thread;
    std::atomic<bool> running{false};
};

// System metrics
struct SystemMetrics {
    double cpuUsage;
    double memoryUsage;
    double avgLatency;
    double maxLatency;
    uint64_t totalMessages;
    uint64_t droppedMessages;
    std::chrono::steady_clock::time_point startTime;
    
    double getUptime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(now - startTime).count();
    }
};

// Main control system class
class ControlSystem {
public:
    explicit ControlSystem(const Config& config = Config{});
    ~ControlSystem();
    
    // Module management
    bool loadModule(const std::string& libraryPath);
    bool unloadModule(const std::string& moduleName);
    std::vector<std::string> getLoadedModules() const;
    
    // Control loop management
    void createControlLoop(const std::string& name, double frequency);
    void setControlFunction(const std::string& loopName, ActuatorCallback func);
    void addSensorToLoop(const std::string& loopName, const std::string& sensorName);
    void addActuatorToLoop(const std::string& loopName, const std::string& actuatorName);
    
    // System control
    void start();
    void stop();
    void emergencyStop();
    bool isRunning() const { return running_; }
    
    // Metrics and monitoring
    void enableMetrics() { metricsEnabled_ = true; }
    void setMetricsCallback(std::function<void(const SystemMetrics&)> callback);
    SystemMetrics getMetrics() const { return metrics_; }
    
    // Module access
    template<typename T>
    std::shared_ptr<T> getModule(const std::string& name) {
        std::lock_guard<std::mutex> lock(modulesMutex_);
        auto it = modules_.find(name);
        if (it != modules_.end()) {
            return std::dynamic_pointer_cast<T>(it->second.module);
        }
        return nullptr;
    }
    
    // Error handling
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
    
private:
    Config config_;
    std::atomic<bool> running_{false};
    std::atomic<bool> metricsEnabled_{false};
    
    // Module storage
    struct ModuleInfo {
        std::shared_ptr<Module> module;
        void* libraryHandle;
        std::string libraryPath;
    };
    
    mutable std::mutex modulesMutex_;
    std::unordered_map<std::string, ModuleInfo> modules_;
    
    // Control loops
    mutable std::mutex loopsMutex_;
    std::unordered_map<std::string, std::unique_ptr<ControlLoop>> controlLoops_;
    
    // IPC components
    std::shared_ptr<MessageQueue> messageQueue_;
    std::shared_ptr<SharedMemory> sharedMemory_;
    
    // Metrics
    mutable SystemMetrics metrics_;
    std::function<void(const SystemMetrics&)> metricsCallback_;
    std::thread metricsThread_;
    
    // Error handling
    ErrorCallback errorCallback_;
    
    // Watchdog
    std::thread watchdogThread_;
    std::atomic<bool> watchdogRunning_{false};
    
    // Internal methods
    void runControlLoop(ControlLoop* loop);
    void updateMetrics();
    void watchdogMonitor();
    bool validateModuleCompatibility(const Module* module);
    void handleError(const std::string& module, const std::string& error);
};

// Exception classes
class ModuleLoadException : public std::runtime_error {
public:
    explicit ModuleLoadException(const std::string& msg) : std::runtime_error(msg) {}
};

class ControlSystemException : public std::runtime_error {
public:
    explicit ControlSystemException(const std::string& msg) : std::runtime_error(msg) {}
};

} // namespace dcs
