#include <iostream>
#include <stdio.h>
#include <string.h>
#include "html_content.h"
#include "common.h"
#include "profiler.h"
#include <set>
#include <unordered_set>
#include <signal.h>
#include <thread>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <mutex>

std::unordered_map<string, unordered_map<string, funcDef*>> funcDef::sFuncMap;
std::vector<funcDef*> funcDef::sFuncList;
std::unordered_map<funcCallTreeNode*, int> treeNodeInFuncTimeCalc::sFuncNodeInTime;
long int totalTraces = 0;
bool writeToHtmlReq = false;
btCircularQ100  backtraceQueue;
std::string outFileName;
bool btDeQueue();

typedef struct _microSecs_t{
    ulong dUs;
    _microSecs_t(ulong t)
    {
        dUs = t;
    }
    operator ulong const() { return dUs; }
}microSecs_t;

ostream& operator << (ostream &xOut, microSecs_t obj)
{
    ulong lTotalUs = obj.dUs; 
    lTotalUs *= 100;
    const char *units = "us";
    if(lTotalUs > 1000) { lTotalUs /= 1000; units = "ms";}
    if(lTotalUs > 1000) { lTotalUs /= 1000; units = "s";}
    xOut<<lTotalUs/100<<"."<<lTotalUs%100<<" "<<units;
    return xOut;
}



funcDef *funcDef::sGetFunc(const char* name, const char* source)
{
    funcDef *lFuncDef = sFuncMap[name][source];
    if(lFuncDef == nullptr)
    {
        lFuncDef = new funcDef(name, source);
        sFuncMap[name][source] = lFuncDef;
        sFuncList.push_back(lFuncDef);
    }
    return lFuncDef;
}

void htmlOutputFtor::operator()(int level, int begin, funcCallTreeNode *xNode)
{
    funcDef *lFunc = xNode->mGetFnDef();
    if(level == 0) {
        d_currThreadProcessing = 0;
        int tid = 0;
        if(sscanf(lFunc->mGetName(), "[Thread %d]", &tid) == 1)
            d_currThreadProcessing = tid;
    }
        
    ulong lCallCount = xNode->mGetCount();
    ulong lChildCount = xNode->mGetChildCount();
    ulong lInTimeCount = treeNodeInFuncTimeCalc::sGetFuncNodeInTime(xNode);
    static ulong sleep_us = std::stol(toolOptions["TRACE_INTERVAL_US"]);
    microSecs_t lCallTime = lCallCount*sleep_us;
    microSecs_t lInTime = lInTimeCount*sleep_us;
    if(begin)
    {
        for(int i = 0; i<level; i++) file<<" ";
        file<<"<li>";
        if(lChildCount)
            file<<"<span class=\"caret\">";
        microSecs_t lFuncTotalTime = lFunc->mGetTotalCount()*sleep_us;
        file<<"("<<lCallTime<<") ("<<lInTime<<") "<<escapeHTMLChars(lFunc->mGetName());
        if(level > 0 && d_currThreadProcessing)
        {
            microSecs_t lFuncThreadTime = lFunc->mGetTotalCountForThread(d_currThreadProcessing)*sleep_us;
            file<<" [Curr Thread Time = "<<lFuncThreadTime<<"]";
        }
        if(level > 0) file<<" [Func Total Time = "<<lFuncTotalTime<<"]";
        if(lChildCount)
            file<<"</span>";
        else
            file<<"</li>";
        file<<"\n";
        if(lChildCount)
        {
            //Marker for new sub-list
            for(int i = 0; i<level; i++) file<<" ";
            file<<"<ul class=\"nested\">\n";
        }
    }
    else
    {
        if(lChildCount)
            file<<"</ul>\n</li>\n";
    }

}
void treeNodeInFuncTimeCalc::operator()(int level, int isPreCall, funcCallTreeNode *xNode)
{
    if(isPreCall)
    {
        int lThisCallCount = xNode->mGetCount();
        int lChildCount = xNode->mGetChildCount();
        for(int i = 0; i < lChildCount; i++)
        {
            lThisCallCount -= xNode->mGetChildByIndex(i)->mGetCount();
        }
        sFuncNodeInTime[xNode] = lThisCallCount;
    }
}
int treeNodeInFuncTimeCalc::sGetFuncNodeInTime(funcCallTreeNode *xNode)
{
    auto it = sFuncNodeInTime.find(xNode);
    if(it != sFuncNodeInTime.end())
        return it->second;
    return 0;
}

funcCallTreeNode::funcCallTreeNode(funcDef *xDef)
{
    dFuncDefPtr = xDef;
    dThisCallCount = 0;
}

funcCallTreeNode *funcCallTreeNode::mGetChildByDef(funcDef *xDef)
{
    funcCallTreeNode *lNode = dChildMap[xDef];
    if(!lNode)
    {
        lNode = new funcCallTreeNode(xDef);
        dChildList.push_back(lNode);
        dChildMap[xDef] = lNode;
    }
    return lNode;
}

funcCallTreeNode *funcCallTreeNode::mGetChildByName(const char *xFuncName, const char *xSrcInfo)
{
    funcDef *lFDef = funcDef::sGetFunc(xFuncName, xSrcInfo);
    return mGetChildByDef(lFDef);
}

funcCallTreeNode *funcCallTreeNode::mGetChildByIndex(int xIndex)
{
    if(xIndex < mGetChildCount())
    {
        return dChildList[xIndex];
    }
    return NULL;
}

void funcCallTreeNode::mPrint(int level = 0)
{
    for(int i = 0; i<level; i++)
        cout<<"  ";
    cout<<"("<<dThisCallCount<<") ";
    dFuncDefPtr->mPrint();
    cout<<endl;
    for(int i = 0; i < dChildList.size(); i++)
        dChildList[i]->mPrint(level+1);
}

void funcCallTreeNode::mTraverse(int level, tdTreeNodeFunctor &xFtor)
{
    xFtor(level, 1, this);
    for(int i = 0; i < dChildList.size(); i++)
        dChildList[i]->mTraverse(level+1, xFtor);
    xFtor(level, 0, this);
}

void funcCallTree::mTraverse(tdTreeNodeFunctor &xFtor)
{
    for(auto it = dTopLevelNode.begin(); it != dTopLevelNode.end(); it++)
        it->second->mTraverse(0, xFtor);
}

void funcCallTree::mAddToHtml(ofstream &htmlFile)
{
    htmlOutputFtor lFtor(htmlFile);
    htmlFile<<"<ul>\n";
    mTraverse(lFtor);
    htmlFile<<"</ul>\n";
}

void funcCallTree::mPrint()
{
    for(auto it = dTopLevelNode.begin(); it != dTopLevelNode.end(); it++)
        it->second->mPrint();
}

void funcCallTree::addCallStack(std::vector<std::string> &xQueue, int tid)
{
    int lSz = xQueue.size();
    if(lSz)
    {
        std::unordered_set<funcDef*> lAlreadyDoneInThisStack; //To prevent multiple addition in case of recursive function
        funcDef *lCurrDef = funcDef::sGetFunc(xQueue[lSz-1].c_str(), "");
        funcCallTreeNode *lCurrTreeNode = dTopLevelNode[lCurrDef];
        if(!lCurrTreeNode)
        {
            lCurrTreeNode = new funcCallTreeNode(lCurrDef);
            dTopLevelNode[lCurrDef] = lCurrTreeNode;
        }
        lCurrTreeNode->mIncrCount(tid);
        lAlreadyDoneInThisStack.insert(lCurrDef);
        for(int i = lSz-2; i >=0; i--)
        {
            lCurrDef = funcDef::sGetFunc(xQueue[i].c_str(), "");  
            bool lIncrDefCnt = (lAlreadyDoneInThisStack.find(lCurrDef) == lAlreadyDoneInThisStack.end()); 
            //if(!lIncrDefCnt) std::cout<<"...."<<lCurrDef->mGetName()<<std::endl;
            lAlreadyDoneInThisStack.insert(lCurrDef);
            lCurrTreeNode = lCurrTreeNode->mGetChildByDef(lCurrDef);
            lCurrTreeNode->mIncrCount(lIncrDefCnt, tid);
        }
    }
}

void funcCallTree::addCallStack(std::vector<funcDef*> &xQueue, int tid)
{
    int lSz = xQueue.size();
    if(lSz)
    {
        std::unordered_set<funcDef*> lAlreadyDoneInThisStack; //To prevent multiple addition in case of recursive function
        funcDef *lCurrDef = xQueue[lSz-1];
        funcCallTreeNode *lCurrTreeNode = dTopLevelNode[lCurrDef];
        if(!lCurrTreeNode)
        {
            lCurrTreeNode = new funcCallTreeNode(lCurrDef);
            dTopLevelNode[lCurrDef] = lCurrTreeNode;
        }
        lAlreadyDoneInThisStack.insert(lCurrDef);
        lCurrTreeNode->mIncrCount();
        for(int i = lSz-2; i >=0; i--)
        {
            lCurrDef = xQueue[i];
            bool lIncrDefCnt = (lAlreadyDoneInThisStack.find(lCurrDef) == lAlreadyDoneInThisStack.end()); 
            lCurrTreeNode = lCurrTreeNode->mGetChildByDef(lCurrDef);
            lCurrTreeNode->mIncrCount(lIncrDefCnt, tid);
            lAlreadyDoneInThisStack.insert(lCurrDef);
        }
    }
}

void writeHtmlHeader(ofstream &out_file)
{
    out_file<<htmlHeader;
}

void writeHtmlProcInfo(ofstream &out_file)
{
    //char cmd[1000];
    //sprintf(cmd, "xargs -0 < /proc/%u/cmdline", getpid());
    std::string procInfo = "??";    //systemOp(cmd);
    out_file<<"Command: "<<procInfo<<"<br>\n";
}

void writeHtmlFooter(ofstream &out_file)
{
    out_file<<htmlFooter;
}

void postTreeCreateProcess(funcCallTree &xTree)
{
    treeNodeInFuncTimeCalc lInTimeCalcFtor;
    xTree.mTraverse(lInTimeCalcFtor);
}

funcCallTree &callTree = *(new funcCallTree);

static std::mutex sBacktraceMutex;
void backtraceCurrThread(int signal)
{
    //std::cerr<<gettid()<<" thread signalled\n";
    std::lock_guard<std::mutex> guard(sBacktraceMutex);  
    traceFunction(&backtraceQueue, 1);
    //std::vector<std::string> currTrace;
    //traceFunction(currTrace, 1);    //Reverse stack
    //std::string threadFn = "[Thread "+std::to_string(gettid())+"]";
    //currTrace.push_back(threadFn);
    //callTree.addCallStack(currTrace);
}

bool btDeQueue()
{
    bool anyDeQued = false;
    while(!backtraceQueue.isEmpty())
    {
        anyDeQued = true;
        backTraceInfo_t bt = backtraceQueue.deQ();
        std::vector<std::string> currTrace;
        btInfoToStrVec(bt, currTrace);           //Reversed stack
        callTree.addCallStack(currTrace, bt.threadId);
    }
    return anyDeQued;
}

void writeProfiledData()
{
    if(!totalTraces) return;
    outFileName = toolOptions["OUTPUT_FILE_PREFIX"] + std::to_string(getpid()) + ".html";
    postTreeCreateProcess(callTree);
    ofstream outFile(outFileName, ios::out);
    writeHtmlHeader(outFile);
    writeHtmlProcInfo(outFile);
    callTree.mAddToHtml(outFile);
    writeHtmlFooter(outFile);
    outFile.close();
    std::cerr<<"Trace written to "<<outFileName<<"\n";
}

void timerInterruptOtherThreads()
{
    sleep(3);
    unsetenv("LD_PRELOAD"); //So that this is not called recursively
    const char *optEnv = getenv("PROFILER_OPTS");
    if(optEnv) readAndParseOptionsCsv(optEnv, toolOptions);
    long sleep_us = std::stol(toolOptions["TRACE_INTERVAL_US"]);
    std::cerr<<"Time interval = "<<sleep_us<<" us\n";
    DIR *proc_dir = opendir("/proc/self/task");
    while(1)
    {
        usleep(sleep_us);
        std::vector<pthread_t> threadIds;
        if(writeToHtmlReq){
            writeToHtmlReq = false;
            writeProfiledData();
        }
        if (proc_dir)
        {
            /* /proc available, iterate through tasks... */
            rewinddir(proc_dir);
            struct dirent *entry;
            while ((entry = readdir(proc_dir)) != NULL)
            {
                if(entry->d_name[0] == '.')
                    continue;
                pthread_t tid = atoi(entry->d_name);
                threadIds.push_back(tid);
            }
        }
        if(threadIds.size()==1) break;
        {
            std::lock_guard<std::mutex> guard(sBacktraceMutex); //Send signal and wait for all pending signals to finish
            for(pthread_t tid:threadIds)
            {
                if(tid == gettid()) continue;
                syscall(SYS_tkill, tid, SIGUSR1);
            }
        }
        btDeQueue();
        ++totalTraces;
        if(totalTraces %30 == 0)
            std::cerr<<totalTraces<<" traces done for pid "<<getpid()<<" ...\n";
        if(totalTraces %60 == 0)
            writeToHtmlReq = true;
    }
    closedir(proc_dir);
}

void sigUsr2Handler(int sig)
{
    if(sig == SIGUSR2) writeToHtmlReq = true;       //Don't do in any of suspended thread
    else               writeProfiledData();
    if(sig == SIGKILL) exit(sig);
}

void __attribute__((constructor)) init()
{
    //std::cerr<<"Hey!! Will log o/p to "<<outFileName<<"\n";
    sleep(2);
    struct sigaction sigact;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = backtraceCurrThread;
    sigaction(SIGUSR1, &sigact, NULL);
    (void) signal(SIGINT, sigUsr2Handler);
    (void) signal(SIGUSR2, sigUsr2Handler);
    (void) signal(SIGSEGV, sigUsr2Handler);
    std::thread th(timerInterruptOtherThreads);
    th.detach();
}

void __attribute__((destructor)) dtor()
{
    writeProfiledData();
}


