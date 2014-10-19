#include "TinyJS_Threading.h"

#undef HAVE_THREADING
#if !defined(NO_THREADING) && !defined(HAVE_CUSTOM_THREADING_IMPL)
#	define HAVE_THREADING
#	ifdef HAVE_CXX_THREADS
#		include <thread>
#	else
#		if defined(WIN32) && !defined(HAVE_PTHREAD)
#			include <windows.h>
#		else
#			include <pthread.h>
#			ifndef HAVE_PTHREAD
#				define HAVE_PTHREAD
#			endif
#		endif
#	endif
#endif

#ifdef HAVE_THREADING 

//////////////////////////////////////////////////////////////////////////
// Mutex
//////////////////////////////////////////////////////////////////////////
#ifndef HAVE_CXX_THREADS

#ifndef HAVE_PTHREAD
// simple mutex 4 windows
//#	define pthread_mutex_t HANDLE
//#	define pthread_mutex_init(m, a)	*(m) = CreateMutex(NULL, false, NULL)
//#	define pthread_mutex_destroy(m)	CloseHandle(*(m));
//#	define pthread_mutex_lock(m)		WaitForSingleObject(*(m), INFINITE);
//#	define pthread_mutex_unlock(m)	ReleaseMutex(*(m));
#	define pthread_mutex_t CRITICAL_SECTION
#	define pthread_mutex_init(m, a)	{ InitializeCriticalSection(m); 0; }
#	define pthread_mutex_destroy(m)	do {} while(0)
#	define pthread_mutex_lock(m)		EnterCriticalSection(m)
#	define pthread_mutex_unlock(m)	LeaveCriticalSection(m)

#endif

class CScriptMutex_impl : public CScriptMutex::CScriptMutex_t {
public:
	CScriptMutex_impl() {
		pthread_mutex_init(&mutex, NULL);
	}
	~CScriptMutex_impl() {
		pthread_mutex_destroy(&mutex);
	}
	void lock() {
		pthread_mutex_lock(&mutex);
	}
	void unlock() {
		pthread_mutex_unlock(&mutex);
	}
	void *getRealMutex() { return &mutex; }
	pthread_mutex_t mutex;
};


CScriptMutex::CScriptMutex() {
	mutex = new CScriptMutex_impl;
}

CScriptMutex::~CScriptMutex() {
	delete mutex;
}

#endif /*HAVE_CXX_THREADS*/

//////////////////////////////////////////////////////////////////////////
// CondVar
//////////////////////////////////////////////////////////////////////////

#ifndef HAVE_CXX_THREADS

#ifndef HAVE_PTHREAD
// simple conditional Variable 4 windows
#	define pthread_cond_t CONDITION_VARIABLE
#	define pthread_cond_init(c, a) InitializeConditionVariable(c)
#	define pthread_cond_destroy(c) do {} while(0)
#	define pthread_cond_wait(c, m) SleepConditionVariableCS(c, m, INFINITE)
#	define pthread_cond_signal(c) WakeConditionVariable(c);
#endif

class CScriptCondVar_impl : public CScriptCondVar::CScriptCondVar_t {
public:
	CScriptCondVar_impl(CScriptCondVar *_this) : This(_this) {
		pthread_cond_init(&cond, NULL);
	}
	~CScriptCondVar_impl() {
		pthread_cond_destroy(&cond);
	}
	CScriptCondVar *This;
	void notify_one() {
		pthread_cond_signal(&cond);
	}
	void wait(CScriptUniqueLock &Lock) {
		pthread_cond_wait(&cond, (pthread_mutex_t *)Lock.mutex->getRealMutex());
	}
	pthread_cond_t cond;
};

CScriptCondVar::CScriptCondVar() {
	condVar = new CScriptCondVar_impl(this);
}
CScriptCondVar::~CScriptCondVar() {
	delete condVar;
}

#endif /*HAVE_CXX_THREADS*/



//////////////////////////////////////////////////////////////////////////
// Threading
//////////////////////////////////////////////////////////////////////////


#ifdef HAVE_CXX_THREADS
#	define pthread_attr_t int
#	define pthread_attr_init(a) do {} while(0)
#	define pthread_attr_destroy(a) do {} while(0)
#	define pthread_t std::thread
#	define pthread_create(t, attr, fnc, a) *(t) = std::thread(fnc, this);
#	define pthread_join(t, v) t.join();
#elif !defined(HAVE_PTHREAD)
// simple pthreads 4 windows
#	define pthread_attr_t SIZE_T
#	define pthread_attr_init(attr) (*attr=0)
#	define pthread_attr_destroy(a) do {} while(0)
#	define pthread_attr_setstacksize(attr, stack) *attr=stack;

#	define pthread_t HANDLE
#	define pthread_create(t, attr, fnc, a) *(t) = CreateThread(NULL, attr ? *((pthread_attr_t*)attr) : 0, (LPTHREAD_START_ROUTINE)fnc, a, 0, NULL)
#	define pthread_join(t, v) WaitForSingleObject(t, INFINITE), GetExitCodeThread(t,(LPDWORD)v), CloseHandle(t)
#endif

class CScriptThread_impl : public CScriptThread::CScriptThread_t {
public:
	CScriptThread_impl(CScriptThread *_this) : retvar((void*)-1), activ(false), running(false), started(false), This(_this) {}
	~CScriptThread_impl() {}
	void Run() {
		if(started) return;
		activ = true;
//		pthread_attr_t attribute;
//		pthread_attr_init(&attribute);
//		pthread_attr_setstacksize(&attribute,1024);
		pthread_create(&thread, NULL /*&attribute*/, (void*(*)(void*))ThreadFnc, this);
//		pthread_attr_destroy(&attribute);
		while(!started);
	}
	int Stop(bool Wait) {
		if(!running) return started ? retValue() : -1;
		activ = false;
		if(Wait) {
			pthread_join(thread, &retvar);
		}
		return (int)retvar;
	}
	int retValue() { return (int)retvar; }
	bool isActiv() { return activ; }
	bool isRunning() { return running; }
	bool isStarted() { return started; }
	static void *ThreadFnc(CScriptThread_impl *This) {
		This->running = This->started = true;
		This->retvar = (void*)This->This->ThreadFnc();
		This->running = false;
		This->This->ThreadFncFinished();
		return This->retvar;
	}
	void *retvar;
	bool activ;
	bool running;
	bool started;
	CScriptThread *This;
	pthread_t thread;
};

CScriptThread::CScriptThread() {
	thread = new CScriptThread_impl(this);
}
CScriptThread::~CScriptThread() {
	delete thread;
}
void CScriptThread::ThreadFncFinished() {}

CScriptCoroutine::StopIteration_t CScriptCoroutine::StopIteration;

bool CScriptCoroutine::next()
{
	if(!isStarted()) {
		Run();
		wake_main.wait();
	} else if(isRunning()) {
		wake_thread.post();
		wake_main.wait();
	} else
		return false;
	if(!isRunning()) return false;
	return true;
}
bool CScriptCoroutine::yield_no_throw() {
	wake_main.post();
	wake_thread.wait();
	return isActiv();
}
void CScriptCoroutine::yield() {
	wake_main.post();
	wake_thread.wait();
	if(!isActiv()) {
		throw StopIteration;
	}
}
int CScriptCoroutine::ThreadFnc() {
	int ret=-1;
	try {
		ret = Coroutine();
	} catch(StopIteration_t &) {
		return 0;
	} catch(std::exception & e) {
		printf("CScriptCoroutine has received an uncaught exception: %s\n", e.what());
		return -1;
	} catch(...) {
		printf("CScriptCoroutine has received an uncaught and unknown exception\n");
		return -1;
	}
	return ret;
}
void CScriptCoroutine::ThreadFncFinished() {
	wake_main.post();
}


#endif // HAVE_THREADING

