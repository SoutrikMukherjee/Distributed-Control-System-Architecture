# Contributing to Distributed Control System

Thank you for your interest in contributing to the Distributed Control System project! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Process](#development-process)
- [Coding Standards](#coding-standards)
- [Testing Guidelines](#testing-guidelines)
- [Submitting Changes](#submitting-changes)
- [Performance Considerations](#performance-considerations)

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](https://www.contributor-covenant.org/). By participating, you are expected to uphold this code.

## Getting Started

1. **Fork the Repository**
   ```bash
   git clone https://github.com/yourusername/distributed-control-system.git
   cd distributed-control-system
   ```

2. **Set Up Development Environment**
   ```bash
   # Install dependencies
   sudo apt-get install build-essential cmake libboost-all-dev libgtest-dev
   
   # Create build directory
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON
   make -j$(nproc)
   ```

3. **Run Tests**
   ```bash
   make test
   ./tests/module_tests  # Run specific test suite
   ```

## Development Process

### Branch Strategy

- `main` - Stable release branch
- `develop` - Integration branch for features
- `feature/*` - Feature branches
- `bugfix/*` - Bug fix branches
- `hotfix/*` - Emergency fixes for production

### Workflow

1. Create a feature branch from `develop`
   ```bash
   git checkout -b feature/your-feature-name develop
   ```

2. Make your changes following our coding standards

3. Write/update tests for your changes

4. Ensure all tests pass and coverage remains above 95%

5. Commit your changes with descriptive messages

6. Push to your fork and create a pull request

## Coding Standards

### C++ Style Guide

We follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html) with these specific requirements:

- **Indentation**: 4 spaces (no tabs)
- **Line Length**: Maximum 100 characters
- **Naming Conventions**:
  - Classes: `PascalCase`
  - Functions: `camelCase`
  - Variables: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Members: `member_name_`

### Code Formatting

Use `clang-format` with the provided `.clang-format` file:

```bash
# Format all files
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# Check formatting
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -n --Werror
```

### Example Code Style

```cpp
namespace dcs {

class ExampleModule : public Module {
public:
    explicit ExampleModule(const Config& config);
    
    void initialize() override;
    
    void processData(const SensorData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!validateData(data)) {
            throw InvalidDataException("Invalid sensor data");
        }
        
        // Process the data
        processed_count_++;
        last_value_ = data.value;
    }
    
private:
    static constexpr double MAX_VALUE = 100.0;
    
    mutable std::mutex mutex_;
    uint64_t processed_count_{0};
    double last_value_{0.0};
    
    bool validateData(const SensorData& data) const;
};

} // namespace dcs
```

## Testing Guidelines

### Test Requirements

- All new code must have corresponding tests
- Maintain minimum 95% code coverage
- Write tests for edge cases and error conditions
- Performance-critical code must include benchmarks

### Test Structure

```cpp
#include <gtest/gtest.h>
#include <dcs/your_module.h>

using namespace dcs;

class YourModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Test setup
    }
    
    void TearDown() override {
        // Cleanup
    }
};

TEST_F(YourModuleTest, DescriptiveTestName) {
    // Arrange
    YourModule module;
    
    // Act
    auto result = module.someOperation();
    
    // Assert
    EXPECT_EQ(result, expected_value);
}

TEST_F(YourModuleTest, HandlesErrorCondition) {
    YourModule module;
    
    EXPECT_THROW(module.invalidOperation(), std::runtime_error);
}
```

### Performance Tests

For performance-critical components:

```cpp
TEST(PerformanceTest, ModuleLatency) {
    const int iterations = 10000;
    YourModule module;
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        module.operation();
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_latency = duration.count() / static_cast<double>(iterations);
    
    EXPECT_LT(avg_latency, 100.0); // Must be under 100μs
}
```

## Submitting Changes

### Commit Messages

Follow the [Conventional Commits](https://www.conventionalcommits.org/) specification:

```
type(scope): subject

body

footer
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Test additions/modifications
- `build`: Build system changes
- `ci`: CI configuration changes

Example:
```
feat(ipc): add priority-based message routing

Implement priority queues for message routing to ensure
high-priority messages are processed first. This reduces
latency for critical control loops by 15%.

Closes #123
```

### Pull Request Process

1. **Title**: Use the same format as commit messages
2. **Description**: Include:
   - Summary of changes
   - Motivation and context
   - How it was tested
   - Performance impact (if applicable)
   - Breaking changes (if any)

3. **Checklist**:
   - [ ] Code follows style guidelines
   - [ ] Tests pass locally
   - [ ] Coverage remains above 95%
   - [ ] Documentation updated
   - [ ] No memory leaks (verified with Valgrind)
   - [ ] Performance benchmarks pass

### Code Review

- Address all review comments
- Keep discussions constructive
- Update PR based on feedback
- Squash commits before merging

## Performance Considerations

### Real-Time Requirements

When contributing to real-time components:

1. **Avoid Dynamic Allocation**: Use pre-allocated buffers
2. **Minimize Locking**: Use lock-free data structures where possible
3. **Predictable Execution**: Avoid algorithms with variable time complexity
4. **Cache Efficiency**: Consider data locality

Example:
```cpp
// Good - Pre-allocated, lock-free
class LockFreeQueue {
    std::array<Message, 1000> buffer_;
    std::atomic<size_t> head_{0};
    std::atomic<size_t> tail_{0};
    
public:
    bool push(const Message& msg) {
        size_t current_tail = tail_.load();
        size_t next_tail = (current_tail + 1) % buffer_.size();
        
        if (next_tail == head_.load()) {
            return false; // Queue full
        }
        
        buffer_[current_tail] = msg;
        tail_.store(next_tail);
        return true;
    }
};
```

### Benchmarking

Add benchmarks for performance-critical code:

```cpp
#include <benchmark/benchmark.h>

static void BM_MessagePassing(benchmark::State& state) {
    MessageQueue queue(state.range(0));
    Message msg;
    
    for (auto _ : state) {
        queue.send(msg);
        benchmark::DoNotOptimize(queue.receive());
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BM_MessagePassing)->Range(8, 8<<10);
```

## Getting Help

- **Issues**: Use GitHub issues for bugs and feature requests
- **Discussions**: Use GitHub Discussions for questions and ideas
- **Documentation**: Check the [API docs](https://yourusername.github.io/distributed-control-system/)
- **Examples**: See the `examples/` directory

Thank you for contributing to make this project better!
