#ifndef __COMMON_H

#define __COMMON_H

#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#define BT_BUF_SIZE 200

typedef struct{
    int threadId = 0;
    int nTraces = 0;
    void *stackTrace[BT_BUF_SIZE];
}backTraceInfo_t;

template <typename T, int N>
class circularQ{
    T arr[N];
    int writePtr = 0;
    int readPtr = 0;
    std::mutex readMutex;
    std::mutex writeMutex;
    public:
        bool isEmpty(){return writePtr == readPtr;}
        bool isFull() {return ((writePtr+1)%N) == readPtr;}
        void enQ(const T&t)
        {
            if(isFull()) return;
            std::lock_guard<std::mutex> guard(writeMutex);
            arr[writePtr] = t;          
            writePtr = (writePtr+1)%N;
        }
        T deQ()
        {
            if(isEmpty()) return T();
            std::lock_guard<std::mutex> guard(readMutex);
            T ret = arr[readPtr];
            readPtr = (readPtr+1)%N;
            return ret;
        }
};

typedef circularQ<backTraceInfo_t, 100> btCircularQ100;
extern std::unordered_map<std::string, std::string> &toolOptions;
void readAndParseOptionsCsv(const char *argsStr, std::unordered_map<std::string, std::string> &argsMap);
void traceFunction(std::vector<std::string>& xTraceOp, int skip=0);
backTraceInfo_t traceFunction(int skip=0);
void traceFunction(btCircularQ100 *btQ,  int skip=0);
void btInfoToStrVec(const backTraceInfo_t &btInfo, std::vector<std::string>& outVec);
std::unordered_map<std::string, std::string> parseCSOpts(std::string csv);
std::pair<int, int> startProcessAndGetInOut(const char *cmd);
std::string systemOp(const char *cmd);
#define COMBINE_TOKEN_1(X,Y) X##Y
#define COMBINE_TOKEN(X,Y) COMBINE_TOKEN_1(X,Y)
template<typename T>
class setValueAndRestoreOnExitHelper{
    T& dOrigVarRef;
    T dOrigValue;

    void *operator new(size_t);
    setValueAndRestoreOnExitHelper(const setValueAndRestoreOnExitHelper&);
    setValueAndRestoreOnExitHelper(setValueAndRestoreOnExitHelper&&);
    public:
        setValueAndRestoreOnExitHelper(T& xOrigVarRef, T xNewVal):dOrigVarRef(xOrigVarRef)
        {
            dOrigValue = dOrigVarRef;   //copy old value
            dOrigVarRef = xNewVal;
        }
        ~setValueAndRestoreOnExitHelper()
        {
            dOrigVarRef = dOrigValue;
        }
};

std::string escapeHTMLChars(const char*);
#define setValueAndRestoreOnExit(obj, newVal) setValueAndRestoreOnExitHelper<decltype(obj)> COMBINE_TOKEN(__internal_obj__,__LINE__)((obj), (newVal))

#endif

