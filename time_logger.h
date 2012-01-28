/*
 * 42TinyJS
 *
 * A fork of TinyJS with the goal to makes a more JavaScript/ECMA compliant engine
 *
 * Authored By Armin Diedering <armin@diedering.de>
 *
 * Copyright (C) 2010 ardisoft
 *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef time_logger_h__
#define time_logger_h__
#if defined(WITH_TIME_LOGGER)

#include <stdint.h>
#include <stdio.h>
#include <string>
#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#endif
class TimeLogger {
public:
	TimeLogger(const char *Name, bool Started=false, const char *extName=0) 
		: 
	name(Name), start_time(gettime()), sum_time(0), calls(0), started(Started) {
		if(extName) {
			name+="[";
			name+=extName;
			name+="]";
		}
	}
	TimeLogger(const char *Name, const char *extName, bool Started=false) 
		: 
	name(Name), start_time(gettime()), sum_time(0), calls(0), started(Started) {
		if(extName) {
			name+="[";
			name+=extName;
			name+="]";
		}
	}
	~TimeLogger() {
		printLog();
//		getchar();
	}
	void startTimer() {
		start_time = gettime();
		started = true;
	}
	void stopTimer() {
		if(!started) return;
		sum_time += gettime()-start_time;
		calls++;
		started = false;
	}
	void printLog() {
		if(started) stopTimer();
		if(calls == 1)
			printf("Timer( %s ) = %d,%06d sec \n",
			name.c_str(), (int)(sum_time / 1000000LL), (int)(sum_time % 1000000LL));
		else if(calls>1)
		printf("Timer( %s ) = %d,%06d sec (called %d times) -> %.d microsec per call\n",
		name.c_str(), (int)(sum_time / 1000000LL), (int)(sum_time % 1000000LL),
		calls, (int)(sum_time/calls));
		calls = 0; sum_time = 0;
	}
private:
//	static int64_t frequenzy = 0;
	std::string name;
	int64_t start_time, sum_time;
	uint32_t calls;
	bool started;
	int64_t gettime() {	// set out to time in millisec
#ifdef _WIN32
		static LARGE_INTEGER fr = {0};
		LARGE_INTEGER li;
		if(fr.QuadPart == 0) QueryPerformanceFrequency(&fr);
		QueryPerformanceCounter(&li);
		return (li.QuadPart * 1000000LL) / fr.QuadPart;
#else
		return (clock() * 1000000LL) / CLOCKS_PER_SEC;
#endif
	};
};
class _TimeLoggerHelper {
public:
	_TimeLoggerHelper(TimeLogger &Tl) : tl(Tl) { tl.startTimer(); }
	~_TimeLoggerHelper() { tl.stopTimer(); }
private:
	TimeLogger &tl;
};
#	define TimeLoggerCreate(a, ...) TimeLogger a##_TimeLogger(#a,##__VA_ARGS__)
#	define TimeLoggerStart(a) a##_TimeLogger.startTimer()
#	define TimeLoggerStop(a) a##_TimeLogger.stopTimer()
#	define TimeLoggerLogprint(a) a##_TimeLogger.printLog()
#	define TimeLoggerHelper(a) _TimeLoggerHelper a##_helper(a##_TimeLogger)
#else /* _DEBUG */
#	define TimeLoggerCreate(...)
#	define TimeLoggerStart(...) do{}while(0)
#	define TimeLoggerStop(...) do{}while(0)
#	define TimeLoggerLogprint(a) do{}while(0)
#	define TimeLoggerHelper(a)  do{}while(0)
#endif /* _DEBUG */



#endif // time_logger_h__
