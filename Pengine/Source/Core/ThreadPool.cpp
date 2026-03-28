#include "ThreadPool.h"

using namespace Pengine;

void ThreadPool::Initialize(size_t threadCount)
{
	for (size_t i = 0; i < threadCount; i++)
	{
		m_Threads.emplace_back([this]
		{
			while (true)
			{
				Task task;
				{
					std::unique_lock<std::mutex> lock(m_Mutex);
					m_RunCondVar.wait(lock, [this]
					{
						return m_IsStoped || !m_Tasks.empty();
					});

					if (m_Tasks.empty() && m_IsStoped)
					{
						break;
					}

					task = std::move(m_Tasks.front());
					m_Tasks.pop();
					m_BusyCount++;
				}

				task();

				{
					std::unique_lock<std::mutex> lock(m_Mutex);
					m_BusyCount--;
				}

				m_WaitCondVar.notify_all();
			}
		});
	}
}

ThreadPool& ThreadPool::GetInstance()
{
	static ThreadPool threadPool;
	return threadPool;
}

void ThreadPool::Shutdown()
{
	WaitIdle();

	{
		std::unique_lock<std::mutex> lock(m_Mutex);
		m_IsStoped = true;
	}

	m_RunCondVar.notify_all();

	for (std::thread& thread : m_Threads)
	{
		thread.join();
	}
}

void ThreadPool::WaitIdle()
{
	std::unique_lock<std::mutex> lock(m_Mutex);
	m_WaitCondVar.wait(lock, [this]
	{
		return m_Tasks.empty() && m_BusyCount == 0;
	});
}
