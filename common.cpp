#include<vector>
#include<string>
#include <execinfo.h>
#include <cxxabi.h>
#include "common.h"
#include <iostream>
#include <stdio.h>
#include <string>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sstream>
#include <signal.h>
#include <thread>
#include <cstring>
#include <poll.h>

std::unordered_map<std::string, std::string> &toolOptions = *(new std::unordered_map<std::string, std::string> {
    {"TRACE_INTERVAL_US", "500000"},
    {"OUTPUT_FILE_PREFIX", "strace_"},
    {"ADDR2LINE_PATH","/usr/local/bin/addr2line"}
});


void readAndParseOptionsCsv(const char *argsStr, std::unordered_map<std::string, std::string> &argsMap){
    std::istringstream argStream(argsStr);
    std::string s;
    while(getline(argStream, s, ','))
    {
        if(s == "") continue;
        std::string key;
        std::string val;
        auto eqPos = s.find('=');
        if(eqPos == std::string::npos)
        {
            key = s;
            val = "true";       //Treated as boolean
        } 
        else
        {
            key = s.substr(0, eqPos);
            val = s.substr(eqPos+1);
        }
        argsMap[key] = val;
    }

}

std::string systemOp(const char *cmd)
{
    char buff[10000];
    FILE *fp = popen(cmd, "r");
    std::string out;
    //const char *preloadedSo = getenv("LD_PRELOAD");
    //unsetenv("LD_PRELOAD"); //So that this is not called recursively
    if(fp != NULL)
    {
        while(fgets(buff, sizeof(buff), fp))
            out += buff;
    }
    //if(preloadedSo) setenv("LD_PRELOAD", preloadedSo, 1); 
    return out;
}

std::pair<int, int> startProcessAndGetInOut(std::string cmd)
{
    const char *preloadedSo = getenv("LD_PRELOAD");
    unsetenv("LD_PRELOAD"); //So that this is not called recursively
    std::cerr<<"Executing cmd: "<<cmd<<"\n";
    int p1[2];
    int p2[2];
    pipe(p1);
    pipe(p2);
    if(fork()==0)
    {
        //Child - modify stdin/out
        dup2(p1[0], STDIN_FILENO);
        dup2(p2[1], STDOUT_FILENO);
        close(p1[1]); close(p2[0]);
        //execl(cmd.c_str(), arg.c_str());
        char *argv[100] = {nullptr};
        char *cmdS = strdup(cmd.c_str());
        char *token = strtok(cmdS, " ");
        char *c = strdup(token);
        int i;
        for(i = 0; i < 100 && token; ++i)
        {
            argv[i] = strdup(token);
            token = strtok(NULL, " ");
        }
        if(i < 100) argv[i] = nullptr;
        else argv[99] = nullptr;


        execv(c, argv);
        std::cerr<<"Cmd finished ("<<cmd<<")\n";
        exit(0);

    }
    close(p1[0]); close(p2[1]);
    if(preloadedSo) setenv("LD_PRELOAD", preloadedSo, 1); 
    return std::pair<int, int>(p2[0], p1[1]);   //Write, read
}

void findAndReplace(std::string &str, const std::string &f, const std::string &replace)
{
    if(f == replace) return;
    std::string::size_type n = 0;
    while((n = str.find(f, n)) != std::string::npos)
    {
        str.replace(n, f.length(), replace);
        n += replace.size();
    }
}

std::string escapeHTMLChars(const char *str)
{
    std::string outStr(str);
    findAndReplace(outStr, "<", "&lt;");
    findAndReplace(outStr, ">", "&gt;");
    return outStr;
}

char *addrToFnName(void *xAddr)
{
    if(!xAddr) return nullptr;
    static std::pair<int, int> addr2LineFd = startProcessAndGetInOut(std::string(toolOptions["ADDR2LINE_PATH"] +" -C -f -s -p -i -e /proc/" + std::to_string(getpid()) + "/exe"));
    static std::unordered_map<void*, char*> addrToNameMap;
    char *fnName = nullptr;
    auto iter = addrToNameMap.find(xAddr);
    if(iter == addrToNameMap.end())
    {
        char **fnNames = backtrace_symbols(&xAddr, 1);
        if(fnNames && fnNames[0])
        {
            fnName = strdup(fnNames[0]);
            char *lFnNameBegin = strchr(fnName, '(');
            char *lFnNameEnd   = lFnNameBegin?strchr(lFnNameBegin+1, '+'):0;
            size_t lFuncNameBufSize = 1024*5;
            char *lFuncNameBuf = (char*) (malloc(lFuncNameBufSize*sizeof(char)));
            if(!lFuncNameBuf){
                std::cerr<<"Unable to allocate memory\n";
                return nullptr;
            }
            if(lFnNameBegin && lFnNameEnd)
            {
                lFnNameBegin++;
                *lFnNameEnd = '\0';
                int lStatus;
                char *lFnName = lFnNameBegin;
                char* lDemangledNamePtr = abi::__cxa_demangle(lFnNameBegin,
                        lFuncNameBuf,
                        &lFuncNameBufSize,
                        &lStatus);
                if(lStatus == 0)
                {
                    lFuncNameBuf = lDemangledNamePtr;
                    lFnName = lFuncNameBuf;
                }
                char *lBracket = strchr(lFnName, '(');
                if(lBracket) *lBracket = '\0';
                fnName = strdup(lFnName);
            }
            else 
                fnName = nullptr;
            free(lFuncNameBuf);
        }
        if(!fnName)
        {
            static const int l_buff_size = 12000;
            static char l_buff[l_buff_size+1];
            struct pollfd addr2lineFdForPoll;
            addr2lineFdForPoll.fd = addr2LineFd.first;
            addr2lineFdForPoll.events = POLLIN;
            addr2lineFdForPoll.revents = 0;
            do{
                addr2lineFdForPoll.revents = 0;
                poll(&addr2lineFdForPoll, 1, 0);
                if(addr2lineFdForPoll.revents == POLLIN) read(addr2LineFd.first, l_buff, l_buff_size);  //Buffer clear
            }while(addr2lineFdForPoll.revents == POLLIN);
            //while(read(addr2LineFd.first, l_buff, 10000));      //Clear stream
            sprintf(l_buff, "0x%lx\n", xAddr);
            write(addr2LineFd.second, l_buff, strlen(l_buff));
            int sz = read(addr2LineFd.first, l_buff, l_buff_size);
            l_buff[l_buff_size] = '\0';
            if(sz)
            {
                char *atStr = strstr(l_buff, " at");
                if(atStr)
                    *atStr = '\0';
                fnName = strdup(l_buff);
            }
            //while(read(addr2LineFd.first, l_buff, 10000) >= 1);      //Clear stream

        }
        if(!fnName){
            static char *unknownFunc = strdup("??");
            fnName = unknownFunc;
        }
        addrToNameMap[xAddr] = fnName;
    }
    else
    {
        fnName = iter->second;
    }
    return fnName;
}

void traceFunction(std::vector<std::string>& xTraceOp, int skip)
{
    int nStackCount = 0;
    void *callAddr[BT_BUF_SIZE];
    char **fnNames;

    nStackCount = backtrace(callAddr, BT_BUF_SIZE);

    for(int i = 2+skip; i < nStackCount; ++i)    //Skip this func and 1 call func
    {
        char *fnName = addrToFnName(callAddr[i]);   
        if(fnName && *fnName) xTraceOp.push_back(fnName);
    }
    std::string threadId = "[Thread " + std::to_string(gettid()) +"]";
    xTraceOp.push_back(threadId);
}

backTraceInfo_t traceFunction(int skip)
{
    backTraceInfo_t btInfo;
    btInfo.nTraces = backtrace(btInfo.stackTrace, BT_BUF_SIZE);
    for(int i = 2+skip; i < btInfo.nTraces; ++i)    //Skip this func and 1 call func
    {
        btInfo.stackTrace[i-(2+skip)] = btInfo.stackTrace[i];
    }
    btInfo.nTraces -= (2+skip);
    btInfo.threadId = gettid();
    return btInfo;
}

void traceFunction(btCircularQ100 *btQ,  int skip)
{
    int sleep = 10;
    while(btQ->isFull() && sleep)
    {
        usleep(100); --sleep;
    }
    if(btQ->isFull()){
        std::cerr<<"BT queue full....BT will be skipped\n";
        return;
    }
    btQ->enQ(traceFunction(skip));
}



void btInfoToStrVec(const backTraceInfo_t &btInfo, std::vector<std::string>& outVec)
{
    for(int i = 0; i < btInfo.nTraces; ++i)  
    {
        char *fnName = addrToFnName(btInfo.stackTrace[i]);   
        if(fnName && *fnName) outVec.push_back(fnName);
    }
    std::string threadFn = "[Thread "+std::to_string(btInfo.threadId)+"]";
    outVec.push_back(threadFn);
}

std::unordered_map<std::string, std::string> parseCSOpts(std::string csv)
{
    std::unordered_map<std::string, std::string> ret;
    return ret;
}
