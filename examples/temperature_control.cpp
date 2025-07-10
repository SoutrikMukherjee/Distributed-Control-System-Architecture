#include <dcs/control_system.h>
#include <dcs/module.h>
#include <iostream>
#include <random>
#include <cmath>

// Example temperature sensor module
class TemperatureSensor : public dcs::SensorModule {
public:
    TemperatureSensor() : SensorModule("TemperatureSensor", "1.0.0") {
        // Simulate initial temperature
        currentTemp_ = 20.0 + (rand() % 10);
    }
    
    void initialize() override {
        std::cout << "[" << getName() << "] Initializing temperature sensor..." << std::endl;
        setUpdateRate(100); // 100Hz sampling rate
        
        // Simulate hardware connection
        connectHardware();
        setState(dcs::ModuleState::READY);
    }
    
    dcs::SensorData read() override {
        // Simulate temperature reading with some noise
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::normal_distribution<> noise(0.0, 0.1);
        
        // Simulate environmental changes
        double environmentalDrift = 0.01 * sin(readCount_ * 0.01);
        currentTemp_ += environmentalDrift + noise(gen);
        
        // Apply heater effect
        currentTemp_ += heaterPower_ * 0.05;
        
        // Natural cooling
        currentTemp_ -= (currentTemp_ - ambientTemp_) * 0.02;
        
        readCount_++;
        
        return dcs::SensorData("temperature", currentTemp_, dcs::Unit::CELSIUS);
    }
    
    void calibrate() override {
        std::cout << "[" << getName() << "] Calibrating sensor..." << std::endl;
        // Simulate calibration process
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        calibrated_ = true;
    }
    
    bool needsCalibration() const override {
        return !calibrated_ || (readCount_ > 10000); // Recalibrate every 10k readings
    }
    
    // Simulate heater feedback (for closed-loop simulation)
    void setHeaterPower(double power) {
        heaterPower_ = power;
    }
    
private:
    double currentTemp_{20.0};
    double ambientTemp_{20.0};
    double heaterPower_{0.0};
    uint64_t readCount_{0};
    bool calibrated_{false};
};

// Example heater actuator module
class HeaterActuator : public dcs::ActuatorModule {
public:
    HeaterActuator() : ActuatorModule("HeaterActuator", "1.0.0") {
        // Set physical limits
        setLimits({0.0, 100.0, 10.0}); // 0-100% power, max 10%/s rate
    }
    
    void initialize() override {
        std::cout << "[" << getName() << "] Initializing heater actuator..." << std::endl;
        connectHardware();
        setState(dcs::ModuleState::READY);
    }
    
    void execute(const dcs::ActuatorCommand& cmd) override {
        if (!validateCommand(cmd)) {
            throw std::runtime_error("Invalid heater command");
        }
        
        if (isEmergencyStopped()) {
            powerLevel_ = 0.0;
            return;
        }
        
        // Apply rate limiting
        double targetPower = cmd.value;
        double maxChange = limits_.maxRate * 0.01; // Convert to per-cycle change
        
        if (std::abs(targetPower - powerLevel_) > maxChange) {
            powerLevel_ += (targetPower > powerLevel_ ? maxChange : -maxChange);
        } else {
            powerLevel_ = targetPower;
        }
        
        // Simulate hardware control
        std::cout << "[" << getName() << "] Heater power: " << powerLevel_ << "%" << std::endl;
        
        // Update metrics
        updateMetrics(0.001); // 1ms processing time
    }
    
    bool isSafeToExecute(const dcs::ActuatorCommand& cmd) const override {
        // Add safety checks
        if (cmd.value > 90.0) { // High power warning
            std::cout << "[WARNING] High heater power requested: " << cmd.value << "%" << std::endl;
        }
        return !isEmergencyStopped() && validateCommand(cmd);
    }
    
    double getPowerLevel() const { return powerLevel_; }
    
private:
    std::atomic<double> powerLevel_{0.0};
};

// PID controller implementation
class PIDController {
public:
    PIDController(double kp, double ki, double kd) 
        : kp_(kp), ki_(ki), kd_(kd) {}
    
    double calculate(double setpoint, double measurement, double dt) {
        double error = setpoint - measurement;
        
        // Proportional term
        double p = kp_ * error;
        
        // Integral term with anti-windup
        integral_ += error * dt;
        integral_ = std::clamp(integral_, -integralLimit_, integralLimit_);
        double i = ki_ * integral_;
        
        // Derivative term with filtering
        double derivative = (error - lastError_) / dt;
        derivative = alpha_ * derivative + (1 - alpha_) * lastDerivative_;
        double d = kd_ * derivative;
        
        lastError_ = error;
        lastDerivative_ = derivative;
        
        return std::clamp(p + i + d, 0.0, 100.0); // Clamp to actuator limits
    }
    
    void reset() {
        integral_ = 0.0;
        lastError_ = 0.0;
        lastDerivative_ = 0.0;
    }
    
private:
    double kp_, ki_, kd_;
    double integral_{0.0};
    double lastError_{0.0};
    double lastDerivative_{0.0};
    double integralLimit_{50.0};
    double alpha_{0.1}; // Derivative filter coefficient
};

// Main example
int main() {
    try {
        // Create control system with custom configuration
        dcs::Config config;
        config.sharedMemorySize = 50 * 1024 * 1024; // 50MB
        config.messageQueueSize = 5000;
        config.enableMetrics = true;
        
        dcs::ControlSystem system(config);
        
        // Create and register modules directly (for demonstration)
        auto tempSensor = std::make_shared<TemperatureSensor>();
        auto heater = std::make_shared<HeaterActuator>();
        
        // In real usage, modules would be loaded from shared libraries:
        // system.loadModule("./libtemperature_sensor.so");
        // system.loadModule("./libheater_actuator.so");
        
        // Initialize modules
        tempSensor->initialize();
        heater->initialize();
        
        // Create PID controller
        PIDController pid(2.0, 0.5, 0.1); // Tuned PID parameters
        double setpoint = 25.0; // Target temperature: 25°C
        
        // Create control loop
        system.createControlLoop("TemperatureControl", 50); // 50Hz control loop
        
        // Define control logic
        auto lastTime = std::chrono::steady_clock::now();
        system.setControlFunction("TemperatureControl", 
            [&pid, &lastTime, setpoint](const dcs::SensorData& input) {
                auto now = std::chrono::steady_clock::now();
                double dt = std::chrono::duration<double>(now - lastTime).count();
                lastTime = now;
                
                double controlOutput = pid.calculate(setpoint, input.value, dt);
                
                std::cout << "Temperature: " << input.value << "°C, "
                          << "Control: " << controlOutput << "%" << std::endl;
                
                return dcs::ActuatorCommand("heater", controlOutput);
            });
        
        // Set up metrics monitoring
        system.setMetricsCallback([](const dcs::SystemMetrics& metrics) {
            std::cout << "\n[METRICS] "
                      << "CPU: " << metrics.cpuUsage << "%, "
                      << "Memory: " << metrics.memoryUsage << "MB, "
                      << "Latency: " << metrics.avgLatency << "μs, "
                      << "Uptime: " << metrics.getUptime() << "s" << std::endl;
        });
        
        // Error handling
        system.setErrorCallback([](const std::string& module, const std::string& error) {
            std::cerr << "[ERROR] Module " << module << ": " << error << std::endl;
        });
        
        // Calibrate sensor
        tempSensor->calibrate();
        
        // Start the system
        std::cout << "\nStarting temperature control system..." << std::endl;
        std::cout << "Target temperature: " << setpoint << "°C" << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;
        
        system.start();
        
        // Simulate temperature feedback loop
        auto startTime = std::chrono::steady_clock::now();
        while (true) {
            // Read temperature
            auto sensorData = tempSensor->read();
            
            // Calculate control
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - lastTime).count();
            lastTime = now;
            
            double controlOutput = pid.calculate(setpoint, sensorData.value, dt);
            
            // Execute control
            heater->execute(dcs::ActuatorCommand("heater", controlOutput));
            
            // Update sensor with heater feedback (simulation only)
            tempSensor->setHeaterPower(heater->getPowerLevel());
            
            // Check if we've reached steady state
            auto elapsed = std::chrono::duration<double>(now - startTime).count();
            if (elapsed > 30.0) { // Run for 30 seconds
                break;
            }
            
            // Control loop delay
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 50Hz
        }
        
        // Stop the system
        std::cout << "\nStopping system..." << std::endl;
        system.stop();
        
        // Print final metrics
        auto finalMetrics = system.getMetrics();
        std::cout << "\nFinal System Metrics:" << std::endl;
        std::cout << "  Total uptime: " << finalMetrics.getUptime() << " seconds" << std::endl;
        std::cout << "  Average latency: " << finalMetrics.avgLatency << " μs" << std::endl;
        std::cout << "  Maximum latency: " << finalMetrics.maxLatency << " μs" << std::endl;
        std::cout << "  Total messages: " << finalMetrics.totalMessages << std::endl;
        std::cout << "  Dropped messages: " << finalMetrics.droppedMessages << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

// Register modules for dynamic loading
DCS_REGISTER_MODULE(TemperatureSensor)
DCS_REGISTER_MODULE(HeaterActuator)
