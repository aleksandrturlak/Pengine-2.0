#pragma once

#include "Core.h"
#include "Logger.h"

#include <condition_variable>
#include <future>
#include <queue>
#include <mutex>
#include <vector>
#include <thread>

namespace Pengine
{

	class PENGINE_API ThreadPool
	{
		using Task = std::move_only_function<void()>;
	public:

		static ThreadPool& GetInstance();

		ThreadPool() = default;
		~ThreadPool() = default;

		ThreadPool(const ThreadPool&) = delete;
		ThreadPool& operator=(const ThreadPool&) = delete;

		void Initialize(size_t threadCount);

		inline size_t GetThreadCount() { return m_Threads.size(); }

		void Shutdown();

		template<typename F, typename ...Args>
		void EnqueueAsync(F&& function, Args&& ...args)
		{
			auto task = [
				function = std::move(function),
				...args = std::move(args)]()
			{
				try
				{
					function(args...);
				}
				catch (...)
				{
					std::rethrow_exception(std::current_exception());
				}
			};

			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_Tasks.emplace(std::move(task));
			}

			m_RunCondVar.notify_one();
		}

		template<typename F, typename ...Args>
		auto EnqueueAsyncFuture(F&& function, Args&& ...args) -> std::future<std::invoke_result_t<F, Args...>>
		{
			using ReturnType = std::invoke_result_t<F, Args...>;
			
			std::packaged_task<ReturnType()> packagedTask(
				[function = std::forward<F>(function),
				...args = std::forward<Args>(args)]() mutable -> ReturnType
				{
					return function(args...);
				}
			);
			
			std::future<ReturnType> future = packagedTask.get_future();
			
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_Tasks.emplace(std::move(packagedTask));
			}
			
			m_RunCondVar.notify_one();
			return future;
		}

		bool IsMainThread() const { return m_MainId == std::this_thread::get_id(); }

		void WaitIdle();

	private:
		std::vector<std::thread> m_Threads;
		std::mutex m_Mutex;
		std::thread::id m_MainId = std::this_thread::get_id();
		std::condition_variable m_RunCondVar;
		std::condition_variable m_WaitCondVar;
		std::queue<Task> m_Tasks;
		int m_BusyCount = 0;
		bool m_IsStoped = false;
	};

}
