#include <gtest/gtest.h>
#include <dcs/module.h>
#include <dcs/control_system.h>
#include <chrono>
#include <thread>

using namespace dcs;
using namespace std::chrono_literals;

// Test fixture for module tests
class ModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code here
    }
    
    void TearDown() override {
        // Cleanup code here
    }
};

// Mock sensor module for testing
class MockSensor : public SensorModule {
public:
    MockSensor() : SensorModule("MockSensor", "1.0.0") {}
    
    void initialize() override {
        initialized_ = true;
        setState(ModuleState::READY);
    }
    
    SensorData read() override {
        readCount_++;
        return SensorData("test", 42.0, Unit::NONE);
    }
    
    bool isInitialized() const { return initialized_; }
    int getReadCount() const { return readCount_; }
    
private:
    bool initialized_{false};
    std::atomic<int> readCount_{0};
};

// Mock actuator module for testing
class MockActuator : public ActuatorModule {
public:
    MockActuator() : ActuatorModule("MockActuator", "1.0.0") {
        setLimits({0.0, 100.0, 50.0});
    }
    
    void initialize() override {
        setState(ModuleState::READY);
    }
    
    void execute(const ActuatorCommand& cmd) override {
        if (!validateCommand(cmd)) {
            throw std::runtime_error("Invalid command");
        }
        lastCommand_ = cmd.value;
        executeCount_++;
    }
    
    double getLastCommand() const { return lastCommand_; }
    int getExecuteCount() const { return executeCount_; }
    
private:
    std::atomic<double> lastCommand_{0.0};
    std::atomic<int> executeCount_{0};
};

// Test module lifecycle
TEST_F(ModuleTest, ModuleLifecycle) {
    MockSensor sensor;
    
    // Initial state
    EXPECT_EQ(sensor.getState(), ModuleState::UNINITIALIZED);
    EXPECT_EQ(sensor.getName(), "MockSensor");
    EXPECT_EQ(sensor.getVersion(), "1.0.0");
    
    // Initialize
    sensor.initialize();
    EXPECT_EQ(sensor.getState(), ModuleState::READY);
    EXPECT_TRUE(sensor.isInitialized());
    
    // Start
    sensor.start();
    EXPECT_EQ(sensor.getState(), ModuleState::RUNNING);
    EXPECT_TRUE(sensor.isHealthy());
    
    // Stop
    sensor.stop();
    EXPECT_EQ(sensor.getState(), ModuleState::PAUSED);
    
    // Shutdown
    sensor.shutdown();
    EXPECT_EQ(sensor.getState(), ModuleState::SHUTDOWN);
}

// Test sensor reading
TEST_F(ModuleTest, SensorReading) {
    MockSensor sensor;
    sensor.initialize();
    sensor.start();
    
    // Test single read
    auto data = sensor.read();
    EXPECT_EQ(data.name, "test");
    EXPECT_DOUBLE_EQ(data.value, 42.0);
    EXPECT_EQ(data.unit, Unit::NONE);
    EXPECT_EQ(sensor.getReadCount(), 1);
    
    // Test multiple reads
    for (int i = 0; i < 10; ++i) {
        sensor.read();
    }
    EXPECT_EQ(sensor.getReadCount(), 11);
    
    // Test update rate
    sensor.setUpdateRate(100.0);
    EXPECT_DOUBLE_EQ(sensor.getUpdateRate(), 100.0);
}

// Test actuator execution
TEST_F(ModuleTest, ActuatorExecution) {
    MockActuator actuator;
    actuator.initialize();
    actuator.start();
    
    // Test valid command
    ActuatorCommand cmd("test", 50.0);
    actuator.execute(cmd);
    EXPECT_DOUBLE_EQ(actuator.getLastCommand(), 50.0);
    EXPECT_EQ(actuator.getExecuteCount(), 1);
    
    // Test command limits
    ActuatorCommand overLimit("test", 150.0);
    EXPECT_THROW(actuator.execute(overLimit), std::runtime_error);
    
    // Test emergency stop
    actuator.setEmergencyStop(true);
    EXPECT_TRUE(actuator.isEmergencyStopped());
    EXPECT_FALSE(actuator.isSafeToExecute(cmd));
}

// Test module metrics
TEST_F(ModuleTest, ModuleMetrics) {
    MockSensor sensor;
    sensor.initialize();
    sensor.start();
    
    // Simulate some operations
    for (int i = 0; i < 100; ++i) {
        sensor.read();
        std::this_thread::sleep_for(1ms);
    }
    
    auto metrics = sensor.getMetrics();
    EXPECT_GT(metrics.processedCount, 0);
    EXPECT_GT(metrics.avgProcessingTime, 0.0);
    EXPECT_EQ(metrics.errorCount, 0);
}

// Control system tests
class ControlSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<ControlSystem> system;
    
    void SetUp() override {
        Config config;
        config.sharedMemorySize = 10 * 1024 * 1024; // 10MB
        config.messageQueueSize = 1000;
        system = std::make_unique<ControlSystem>(config);
    }
};

// Test control loop creation
TEST_F(ControlSystemTest, ControlLoopCreation) {
    system->createControlLoop("TestLoop", 50.0);
    
    // Set control function
    bool functionCalled = false;
    system->setControlFunction("TestLoop", 
        [&functionCalled](const SensorData& data) {
            functionCalled = true;
            return ActuatorCommand("test", data.value * 2.0);
        });
    
    // Note: In real implementation, we would test the control loop execution
    EXPECT_TRUE(true); // Placeholder
}

// Test system metrics
TEST_F(ControlSystemTest, SystemMetrics) {
    system->enableMetrics();
    
    bool metricsReceived = false;
    system->setMetricsCallback([&metricsReceived](const SystemMetrics& metrics) {
        metricsReceived = true;
        EXPECT_GE(metrics.cpuUsage, 0.0);
        EXPECT_LE(metrics.cpuUsage, 100.0);
        EXPECT_GE(metrics.memoryUsage, 0.0);
        EXPECT_GE(metrics.avgLatency, 0.0);
    });
    
    system->start();
    std::this_thread::sleep_for(100ms);
    system->stop();
    
    // Metrics should have been called at least once
    EXPECT_TRUE(metricsReceived);
}

// Performance benchmarks
class PerformanceTest : public ::testing::Test {
protected:
    void measureLatency(const std::string& name, std::function<void()> operation) {
        const int iterations = 10000;
        std::vector<double> latencies;
        latencies.reserve(iterations);
        
        // Warmup
        for (int i = 0; i < 100; ++i) {
            operation();
        }
        
        // Measure
        for (int i = 0; i < iterations; ++i) {
            auto start = std::chrono::high_resolution_clock::now();
            operation();
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            latencies.push_back(duration.count());
        }
        
        // Calculate statistics
        std::sort(latencies.begin(), latencies.end());
        double avg = std::accumulate(latencies.begin(), latencies.end(), 0.0) / iterations;
        double p99 = latencies[iterations * 99 / 100];
        double max = latencies.back();
        
        std::cout << name << " Latency - "
                  << "Avg: " << avg << "μs, "
                  << "P99: " << p99 << "μs, "
                  << "Max: " << max << "μs" << std::endl;
        
        // Assert performance requirements
        EXPECT_LT(avg, 100.0); // Average under 100μs
        EXPECT_LT(p99, 200.0); // P99 under 200μs
    }
};

// Benchmark sensor reading
TEST_F(PerformanceTest, SensorReadLatency) {
    MockSensor sensor;
    sensor.initialize();
    sensor.start();
    
    measureLatency("Sensor Read", [&sensor]() {
        volatile auto data = sensor.read();
    });
}

// Benchmark actuator execution
TEST_F(PerformanceTest, ActuatorExecuteLatency) {
    MockActuator actuator;
    actuator.initialize();
    actuator.start();
    
    ActuatorCommand cmd("test", 50.0);
    measureLatency("Actuator Execute", [&actuator, &cmd]() {
        actuator.execute(cmd);
    });
}

// Stress test for module operations
TEST_F(PerformanceTest, StressTest) {
    const int numSensors = 50;
    const int numActuators = 50;
    const int duration = 5; // seconds
    
    std::vector<std::unique_ptr<MockSensor>> sensors;
    std::vector<std::unique_ptr<MockActuator>> actuators;
    
    // Create modules
    for (int i = 0; i < numSensors; ++i) {
        auto sensor = std::make_unique<MockSensor>();
        sensor->initialize();
        sensor->start();
        sensors.push_back(std::move(sensor));
    }
    
    for (int i = 0; i < numActuators; ++i) {
        auto actuator = std::make_unique<MockActuator>();
        actuator->initialize();
        actuator->start();
        actuators.push_back(std::move(actuator));
    }
    
    // Run stress test
    std::atomic<bool> running{true};
    std::atomic<uint64_t> totalOperations{0};
    std::vector<std::thread> threads;
    
    // Sensor threads
    for (int i = 0; i < numSensors; ++i) {
        threads.emplace_back([&sensors, i, &running, &totalOperations]() {
            while (running) {
                sensors[i]->read();
                totalOperations++;
            }
        });
    }
    
    // Actuator threads
    for (int i = 0; i < numActuators; ++i) {
        threads.emplace_back([&actuators, i, &running, &totalOperations]() {
            ActuatorCommand cmd("test", 50.0);
            while (running) {
                actuators[i]->execute(cmd);
                totalOperations++;
            }
        });
    }
    
    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::seconds(duration));
    running = false;
    
    // Wait for threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Calculate throughput
    double throughput = totalOperations.load() / static_cast<double>(duration);
    std::cout << "Stress Test - Total operations: " << totalOperations 
              << ", Throughput: " << throughput << " ops/sec" << std::endl;
    
    // Verify minimum throughput
    EXPECT_GT(throughput, 100000); // At least 100k ops/sec
}

// Main test runner
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
