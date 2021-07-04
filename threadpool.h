#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "threadtask.h"

typedef std::chrono::steady_clock SteadyClock;

class ThreadPool
{
protected:
	using FunctionType = std::function<void()>;
	using FunctionTypeVector = std::vector<FunctionType>;

	std::uint32_t batchSize = 1;

	std::priority_queue<ThreadTask, ThreadTaskVector, std::less<ThreadTask>> timedTaskQueue;
	std::mutex timedTaskQueueMutex;

	std::thread timedTaskThread;

	std::priority_queue<ThreadTask, ThreadTaskVector> workerTaskQueue;
	std::mutex workerTaskQueueMutex;

	std::vector<std::thread> workerThreadList;
	std::mutex workerThreadListMutex;

	std::uint32_t workerThreadCount;

	bool paused;
	bool running;

	void createThreads();
	void destroyThreads();

	bool getNextTask(FunctionType& task);
	bool getNextTimedTask(FunctionType& task);
	bool getNextTaskBatch(ThreadTaskVector& taskVector);

	bool hasTasks();
	bool hasTimedTasks();

	void timedWorker();

	void waitForEmptyQueue();
	void worker();
public:
	ThreadPool();
	~ThreadPool();

	template <typename T, typename... A>
	void addTask(ThreadTaskPriority priority, const T& task, const A&...args)
	{
		ThreadTask threadTask{ priority,[task, args...] {
				task(args...);
			} };

		std::scoped_lock lock(this->workerTaskQueueMutex);

		this->workerTaskQueue.push(threadTask);
	}

	template <typename T, typename... A>
	void addTimedTask(std::uint32_t inMilliseconds, const T& task, const A&...args)
	{
		std::time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch()).count();

		ThreadTask threadTask{ static_cast<ThreadTaskPriority>(now + inMilliseconds),[task, args...] {
				task(args...);
			} };

		std::scoped_lock lock(this->timedTaskQueueMutex);

		this->timedTaskQueue.push(threadTask);
	}

	std::uint32_t getThreadCount();

	bool isPaused();
	bool isRunning();

	void pause();

	void setBatchSize(std::uint32_t batchSize);
	void setThreadCount(std::uint32_t threadCount);

	void stop();

	void unpause();
};
