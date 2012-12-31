#include "TinyJS_Threading.h"

#undef HAVE_THREADING
#if !defined(NO_THREADING) && !defined(HAVE_CUSTOM_THREADING_IMPL)
#	define HAVE_THREADING
#	if defined(WIN32) && !defined(HAVE_PTHREAD)
#		include <windows.h>
#	else
#		include <pthread.h>
#	endif
#endif

#ifdef HAVE_THREADING 

#ifndef HAVE_PTHREAD
// simple pthreads 4 windows
#	define pthread_t HANDLE
#	define pthread_create(t, stack, fnc, a) *(t) = CreateThread(NULL, stack, (LPTHREAD_START_ROUTINE)fnc, a, 0, NULL)
#	define pthread_join(t, v) WaitForSingleObject(t, INFINITE), GetExitCodeThread(t,(LPDWORD)v), CloseHandle(t)

#	define pthread_mutex_t HANDLE
#	define pthread_mutex_init(m, a)	*(m) = CreateMutex(NULL, false, NULL)
#	define pthread_mutex_destroy(m)	CloseHandle(*(m));
#	define pthread_mutex_lock(m)		WaitForSingleObject(*(m), INFINITE);
#	define pthread_mutex_unlock(m)	ReleaseMutex(*(m));
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
	pthread_mutex_t mutex;
};

CScriptMutex::CScriptMutex() {
	mutex = new CScriptMutex_impl;
}

CScriptMutex::~CScriptMutex() {
	delete mutex;
}

class CScriptThread_impl : public CScriptThread::CScriptThread_t {
public:
	CScriptThread_impl(CScriptThread *_this) : activ(false), running(false), This(_this) {}
	~CScriptThread_impl() {}
	void Run() {
		if(running) return;
		activ = true;
		pthread_create(&thread, NULL, (void*(*)(void*))ThreadFnc, this);
		while(!running);
	}
	int Stop() {
		if(!running) return -1;
		void *retvar;
		activ = false;
		pthread_join(thread, &retvar);
		running = false;
		return (int)retvar;
	}
	bool isActiv() { return activ; }
	static void *ThreadFnc(CScriptThread_impl *This) {
		This->running = true;
		return (void*) This->This->ThreadFnc();
	}
	bool activ;
	bool running;
	CScriptThread *This;
	pthread_t thread;
};

CScriptThread::CScriptThread() {
	thread = new CScriptThread_impl(this);
}
CScriptThread::~CScriptThread() {
	delete thread;
}
#endif // HAVE_THREADING