#pragma once

#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <vector>
#include <atomic>
#include <variant>

namespace dcs {

// Forward declarations
class ControlSystem;
class MessageQueue;
class SharedMemory;

// Data types
enum class Unit {
    NONE,
    CELSIUS,
    FAHRENHEIT,
    METERS,
    MILLIMETERS,
    RADIANS,
    DEGREES,
    NEWTONS,
    PASCALS,
    VOLTS,
    AMPERES,
    WATTS
};

struct SensorData {
    std::string name;
    double value;
    Unit unit;
    std::chrono::steady_clock::time_point timestamp;
    
    SensorData(const std::string& n, double v, Unit u = Unit::NONE)
        : name(n), value(v), unit(u), timestamp(std::chrono::steady_clock::now()) {}
};

struct ActuatorCommand {
    std::string target;
    double value;
    Unit unit;
    
    ActuatorCommand(const std::string& t, double v, Unit u = Unit::NONE)
        : target(t), value(v), unit(u) {}
};

// Module states
enum class ModuleState {
    UNINITIALIZED,
    INITIALIZING,
    READY,
    RUNNING,
    PAUSED,
    ERROR,
    SHUTDOWN
};

// Base module class
class Module {
public:
    Module(const std::string& name, const std::string& version);
    virtual ~Module() = default;
    
    // Lifecycle methods
    virtual void initialize() = 0;
    virtual void start();
    virtual void stop();
    virtual void shutdown();
    
    // State management
    ModuleState getState() const { return state_; }
    const std::string& getName() const { return name_; }
    const std::string& getVersion() const { return version_; }
    
    // Health check
    virtual bool isHealthy() const { return state_ == ModuleState::RUNNING; }
    
    // Metrics
    struct Metrics {
        uint64_t processedCount{0};
        double avgProcessingTime{0.0};
        double maxProcessingTime{0.0};
        uint64_t errorCount{0};
        double uptime{0.0};
    };
    
    Metrics getMetrics() const { return metrics_; }
    
protected:
    std::string name_;
    std::string version_;
    std::atomic<ModuleState> state_{ModuleState::UNINITIALIZED};
    mutable Metrics metrics_;
    
    // IPC handles
    std::shared_ptr<MessageQueue> messageQueue_;
    std::shared_ptr<SharedMemory> sharedMemory_;
    
    // Helper methods
    void setState(ModuleState state) { state_ = state; }
    void updateMetrics(double processingTime);
    
private:
    friend class ControlSystem;
    void setIPCHandles(std::shared_ptr<MessageQueue> mq, std::shared_ptr<SharedMemory> sm);
};

// Sensor module specialization
class SensorModule : public Module {
public:
    using Module::Module;
    
    // Sensor-specific interface
    virtual SensorData read() = 0;
    void setUpdateRate(double hz);
    double getUpdateRate() const { return updateRate_; }
    
    // Calibration support
    virtual void calibrate() {}
    virtual bool needsCalibration() const { return false; }
    
protected:
    double updateRate_{10.0}; // Default 10Hz
    
    // Hardware interface helpers
    virtual void connectHardware() {}
    virtual void disconnectHardware() {}
};

// Actuator module specialization
class ActuatorModule : public Module {
public:
    using Module::Module;
    
    // Actuator-specific interface
    virtual void execute(const ActuatorCommand& cmd) = 0;
    
    // Safety features
    virtual bool isSafeToExecute(const ActuatorCommand& cmd) const;
    void setEmergencyStop(bool stop) { emergencyStop_ = stop; }
    bool isEmergencyStopped() const { return emergencyStop_; }
    
    // Limits
    struct Limits {
        double minValue{-std::numeric_limits<double>::max()};
        double maxValue{std::numeric_limits<double>::max()};
        double maxRate{std::numeric_limits<double>::max()}; // units/second
    };
    
    void setLimits(const Limits& limits) { limits_ = limits; }
    Limits getLimits() const { return limits_; }
    
protected:
    std::atomic<bool> emergencyStop_{false};
    Limits limits_;
    
    // Command validation
    bool validateCommand(const ActuatorCommand& cmd) const;
};

// Module registration macro
#define DCS_REGISTER_MODULE(ModuleClass) \
    extern "C" { \
        dcs::Module* createModule() { \
            return new ModuleClass(); \
        } \
        void destroyModule(dcs::Module* module) { \
            delete module; \
        } \
        const char* getModuleInfo() { \
            return #ModuleClass; \
        } \
    }

// Callback types
using SensorCallback = std::function<void(const SensorData&)>;
using ActuatorCallback = std::function<ActuatorCommand(const SensorData&)>;
using ErrorCallback = std::function<void(const std::string& module, const std::string& error)>;

} // namespace dcs
