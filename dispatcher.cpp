// dispatcher.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include <Windows.h>
#include <iostream>
#include <process.h>

#include <vector>
#include "boost/shared_ptr.hpp"
#include "boost/make_shared.hpp"
#include "boost/function.hpp"
#include "boost/bind.hpp"
#include "boost/foreach.hpp"

class CriticalSectionLocker
{
private:
	CRITICAL_SECTION &cs_;
public:
	explicit CriticalSectionLocker(CRITICAL_SECTION &cs)
		:cs_(cs)
	{
		EnterCriticalSection(&cs_);
	}
	
	~CriticalSectionLocker()
	{
		LeaveCriticalSection(&cs_);
	}
};

class ThreadDispatcher
{
public:
	typedef struct _DISPATCHERINFO
	{
		boost::function<void(ThreadDispatcher*)> dispatcher_callback;
		unsigned long start_ms;
		unsigned long tick_ms;
		PVOID return_address;
	}DISPATCHERINFO, *PDISPATCHERINFO;
private:
	typedef std::vector<DISPATCHERINFO> DispatcherVec;
	typedef std::vector<DISPATCHERINFO>::iterator DispatcherVit;

	DispatcherVec dispatcher_vec_;
	DispatcherVec ready_dispatcher_vec_;

private:
	CRITICAL_SECTION dispatcher_cs_;
	CRITICAL_SECTION ready_dispatcher_cs_;
	std::vector<HANDLE> thread_handle_vec_;
	BOOL dispatcher_exit_;

private:
	void CriticalSectionInitialize()
	{
		InitializeCriticalSection(&dispatcher_cs_);
		InitializeCriticalSection(&ready_dispatcher_cs_);
	}
	void CriticalSectionCleanup()
	{
		DeleteCriticalSection(&dispatcher_cs_);
		DeleteCriticalSection(&ready_dispatcher_cs_);
	}

	BOOL ReadyDispatchExist() const
	{
		return (!ready_dispatcher_vec_.empty());
	}

	BOOL DispatcherExitCheck() const
	{
		return dispatcher_exit_;
	}

	void Wait(DWORD delay_value)
	{
		Sleep(delay_value);
	}

	DWORD GetDelayValue() const
	{
		return 100;
	}

	VOID ReadyDispatcherHandling()
	{
		CriticalSectionLocker locker(ready_dispatcher_cs_);
		DispatcherVit it = ready_dispatcher_vec_.begin();
		DispatcherVit end = ready_dispatcher_vec_.end();
		DWORD count = 0;
		for (; it != end;++it)
		{
			(*it).dispatcher_callback(this);
		}
		ready_dispatcher_vec_.clear();
	}

	VOID DispatcherHandling()
	{
		CriticalSectionLocker locker(dispatcher_cs_);
		
		DispatcherVit it = dispatcher_vec_.begin();
		for (; it != dispatcher_vec_.end();)
		{
			unsigned long current_tick = GetTickCount() - (*it).start_ms;
			if (current_tick >= (*it).tick_ms)
			{
				CriticalSectionLocker ready_locker(ready_dispatcher_cs_);
				ready_dispatcher_vec_.push_back((*it));
				it = dispatcher_vec_.erase(it);
			}
			else
				++it;
		}
	}

	static unsigned int __stdcall DispatcherThreadProc(void *param)
	{
		ThreadDispatcher *thread_dispatcher = static_cast<ThreadDispatcher *>(param);
		for (;;)
		{
			thread_dispatcher->Wait(thread_dispatcher->GetDelayValue());

			if (thread_dispatcher->DispatcherExitCheck())
				break;

			thread_dispatcher->DispatcherHandling();

			if (thread_dispatcher->ReadyDispatchExist())
			{
				thread_dispatcher->ReadyDispatcherHandling();
			}
		}
		return 0;
	}

public:
	ThreadDispatcher()
	{
		CriticalSectionInitialize();
		dispatcher_exit_ = FALSE;
		Run();
	}
	virtual ~ThreadDispatcher(void)
	{
		Stop();
	}

	void Run()
	{
		ULONG threadid = 0;;
		
		for (int i = 0; i < 4; i++)
		{
			HANDLE thread_handle = (HANDLE)_beginthreadex(NULL, NULL, DispatcherThreadProc, this, 0, (unsigned int *)&threadid);
			thread_handle_vec_.push_back(thread_handle);
		}
	}

	void Stop()
	{
		BOOST_FOREACH(const HANDLE dispatcher_thread_handle, thread_handle_vec_)
		{
			if (WaitForSingleObject(dispatcher_thread_handle, 5000) != WAIT_OBJECT_0)
				TerminateThread(dispatcher_thread_handle, 0);

			CloseHandle(dispatcher_thread_handle);
		}
	}

	void Suspend()
	{
		BOOST_FOREACH(const HANDLE dispatcher_thread_handle, thread_handle_vec_)
		{
			SuspendThread(dispatcher_thread_handle);
		}
	}

	void Resume()
	{
		BOOST_FOREACH(const HANDLE dispatcher_thread_handle, thread_handle_vec_)
		{
			ResumeThread(dispatcher_thread_handle);
		}
	}

	void __stdcall Add(unsigned long tick, boost::function<void(ThreadDispatcher*)> callback)
	{
		DISPATCHERINFO dispatcher_info;

		dispatcher_info.return_address	= _ReturnAddress();
		dispatcher_info.tick_ms = tick;
		dispatcher_info.start_ms = GetTickCount();
		dispatcher_info.dispatcher_callback = callback;

		CriticalSectionLocker locker(dispatcher_cs_);
		dispatcher_vec_.push_back(dispatcher_info);
	}
};

void DispatcherCallback(ThreadDispatcher *thread_dispatcher)
{
	printf("asd\r\n");
	thread_dispatcher->Add(4000, DispatcherCallback);
}

int _tmain(int argc, _TCHAR* argv[])
{
	boost::shared_ptr<ThreadDispatcher> thread_dispatcher_shared_ptr;
	thread_dispatcher_shared_ptr = boost::make_shared<ThreadDispatcher>();

	for (int i = 0; i < 200; i++)
		thread_dispatcher_shared_ptr->Add(1000, DispatcherCallback);

	for (;;){}
	return 0;
}

