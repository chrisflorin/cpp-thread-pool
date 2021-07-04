#include <climits>
#include <iostream>
#include <thread>

#include "threadpool.h"

ThreadPool::ThreadPool()
{
	this->paused = false;
	this->running = true;

	this->setThreadCount(std::thread::hardware_concurrency());

	this->timedTaskThread = std::thread(&ThreadPool::timedWorker, this);
}

ThreadPool::~ThreadPool()
{
	this->stop();
}

void ThreadPool::createThreads()
{
	if (this->getThreadCount() > this->workerThreadList.size()) {
		for (std::vector<std::thread>::size_type i = this->workerThreadList.size(); i < this->getThreadCount(); i++) {
			{
				std::scoped_lock lock(this->workerThreadListMutex);

				this->workerThreadList.push_back(std::thread(&ThreadPool::worker, this));
			}
		}
	}
	else if (this->getThreadCount() < this->workerThreadList.size()) {
		this->destroyThreads();
	}
}

void ThreadPool::destroyThreads()
{
	for (std::vector<std::thread>::size_type i = this->workerThreadList.size(); i > this->getThreadCount(); i--) {
		std::scoped_lock lock(this->workerThreadListMutex);

		std::thread& oldThread = this->workerThreadList.back();

		if (oldThread.joinable()) {
			oldThread.join();
		}

		this->workerThreadList.pop_back();
	}
}

bool ThreadPool::getNextTask(FunctionType& task)
{
	std::scoped_lock lock(this->workerTaskQueueMutex);

	if (!this->hasTasks()) {
		return false;
	}

	ThreadTask threadTask = this->workerTaskQueue.top();
	this->workerTaskQueue.pop();

	task = threadTask.function;

	return true;
}

bool ThreadPool::getNextTimedTask(FunctionType& task)
{
	std::scoped_lock lock(this->timedTaskQueueMutex);

	if (!this->hasTimedTasks()) {
		return false;
	}

	ThreadTask threadTask = this->timedTaskQueue.top();

	std::time_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();

	if (threadTask.priority > now) {
		return false;
	}

	this->timedTaskQueue.pop();

	task = threadTask.function;

	return true;
}

bool ThreadPool::getNextTaskBatch(ThreadTaskVector& taskVector)
{
	std::scoped_lock lock(this->workerTaskQueueMutex);

	for (std::uint32_t i = 0; i < this->batchSize; i++) {
		if (!this->hasTasks()) {
			return i > 0;
		}

		ThreadTask threadTask = this->workerTaskQueue.top();
		this->workerTaskQueue.pop();

		taskVector.push_back(threadTask);
	}

	return true;
}

std::uint32_t ThreadPool::getThreadCount()
{
	return this->workerThreadCount;
}

bool ThreadPool::hasTasks()
{
	return !this->workerTaskQueue.empty();
}

bool ThreadPool::hasTimedTasks()
{
	return !this->timedTaskQueue.empty();
}

bool ThreadPool::isPaused()
{
	return this->paused;
}

bool ThreadPool::isRunning()
{
	return this->running;
}

void ThreadPool::pause()
{
	this->paused = true;
}

void ThreadPool::setBatchSize(std::uint32_t batchSize)
{
	this->batchSize = batchSize;
}

void ThreadPool::setThreadCount(std::uint32_t threadCount)
{
	this->workerThreadCount = threadCount;

	this->createThreads();
}

void ThreadPool::stop()
{
	this->running = false;

	this->waitForEmptyQueue();

	this->setThreadCount(0);

	this->timedTaskThread.join();
}

void ThreadPool::timedWorker()
{
	FunctionType task;

	do {
		while (this->getNextTimedTask(task)) {
			this->addTask(ULLONG_MAX, task);
		}

		//std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	} while (this->isRunning());
}

void ThreadPool::unpause()
{
	this->paused = false;
}

void ThreadPool::waitForEmptyQueue()
{
	while (this->hasTasks()) {
		std::this_thread::yield();
	}
}

void ThreadPool::worker()
{
	ThreadTaskVector taskVector;

	do {
		while (!isPaused()
			&& this->getNextTaskBatch(taskVector)) {
			for (ThreadTaskVector::iterator it = taskVector.begin(); it != taskVector.end(); ++it) {
				ThreadTask& task = (*it);
				std::invoke(task.function);
			}

			taskVector.clear();
		}

		//std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	} while (this->isRunning());
}
