#pragma once
#include <string>

enum class TaskStatus
{
    Pending,
    Running,
    Success,
    Failed,
    Retrying
};

struct TaskResult
{
    TaskStatus status = TaskStatus::Pending;
    std::string message;
    std::string outputPath;
    double durationMs = 0.0;
};
