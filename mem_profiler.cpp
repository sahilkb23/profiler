#include <iostream>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include "html_content.h"
#include "mem_profiler.h"
#include <execinfo.h>
#include <cxxabi.h>
#include <algorithm>
#include <unistd.h>
#include <set>


std::unordered_map<string, funcDef*> funcDef::sFuncMap;
std::vector<funcDef*> funcDef::sFuncList;
std::map<funcCallTreeNode*, ullong> treeNodeInFuncTimeCalc::sFuncNodeInTime;

funcDef *funcDef::sGetFunc(const char* name, const char* source)
{
    if(!name || *name == '\0')
        name = "unknown";
    funcDef *lFuncDef = nullptr;
    auto lIt = sFuncMap.find(name);    ;
    if(lIt == sFuncMap.end())
    {
        lFuncDef = new funcDef(name, source);
        sFuncMap.insert(std::pair<std::string, funcDef*>(name, lFuncDef));
        sFuncList.push_back(lFuncDef);
    }
    else
        lFuncDef = lIt->second;
    return lFuncDef;
}

typedef struct _memorySz{
    ullong dBytes;
    _memorySz(ullong xBytes)
    {
        dBytes = xBytes;
    }
    operator ullong const() { return dBytes; }
}memorySz;

_memorySz operator "" _KB(ullong xKb)
{
    return memorySz(xKb*1024);
}
_memorySz operator "" _MB(ullong xMb)
{
    return memorySz(xMb*1024*1024);
}
_memorySz operator "" _GB(ullong xGb)
{
    return memorySz(xGb*1024*1024*1024);
}


ostream& operator << (ostream &xOut, memorySz obj)
{
    ullong lTotalMem = obj.dBytes; 
    lTotalMem *= 100;   //2 decimal places
    const char *units = "bytes";
    if(lTotalMem > 102400) { lTotalMem /= 1024; units = "KB";}
    if(lTotalMem > 102400) { lTotalMem /= 1024; units = "MB";}
    if(lTotalMem > 102400) { lTotalMem /= 1024; units = "GB";}
    xOut<<lTotalMem/100<<"."<<lTotalMem%100<<" "<<units;
    return xOut;
}

void htmlOutputFtor::operator()(int level, int begin, funcCallTreeNode *xNode)
{
    memorySz lTotalMem(xNode->mGetCount());
    int lChildCount = xNode->mGetChildCount();
    memorySz lInTimeCount(treeNodeInFuncTimeCalc::sGetFuncNodeInTime(xNode));
    if(begin)
    {
        funcDef *lFunc = xNode->mGetFnDef();
        memorySz lTotalFuncMem(lFunc->mGetTotalCount());
        for(int i = 0; i<level; i++) file<<" ";
        file<<"<li>";
        if(lChildCount)
            file<<"<span class=\"caret\">";
        file<<"("<<lTotalMem<<")("<<lInTimeCount<<") "<<escapeHTMLChars(lFunc->mGetName())<<" [Total Allocations = "<<lTotalFuncMem<<"]";
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
        ullong lThisCallCount = xNode->mGetCount();
        int lChildCount = xNode->mGetChildCount();
        for(int i = 0; i < lChildCount; i++)
        {
            lThisCallCount -= xNode->mGetChildByIndex(i)->mGetCount();
        }
        sFuncNodeInTime[xNode] = lThisCallCount;
    }
}
ullong treeNodeInFuncTimeCalc::sGetFuncNodeInTime(funcCallTreeNode *xNode)
{
    auto it = sFuncNodeInTime.find(xNode);
    if(it != sFuncNodeInTime.end())
        return it->second;
    return 0;
}


funcCallTreeNode::funcCallTreeNode(funcDef *xDef)
{
    dFuncDefPtr = xDef;
    dThisCallAllocBytes = 0;
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
    cout<<"("<<dThisCallAllocBytes<<") ";
    dFuncDefPtr->mPrint();
    cout<<endl;
    for(int i = 0; i < dChildList.size(); i++)
        dChildList[i]->mPrint(level+1);
}

void funcCallTreeNode::mTraverse(int level, tdTreeNodeFunctor &xFtor, tdIsGraterFnForSort isGraterFnForSort)
{
    xFtor(level, 1, this);
    if(isGraterFnForSort)
        std::sort(dChildList.begin(), dChildList.end(), isGraterFnForSort);
    for(int i = 0; i < dChildList.size(); i++)
        dChildList[i]->mTraverse(level+1, xFtor, isGraterFnForSort);
    xFtor(level, 0, this);
}

void funcCallTree::mTraverse(tdTreeNodeFunctor &xFtor, tdIsGraterFnForSort isGraterFnForSort)
{
    for(auto it = dTopLevelNode.begin(); it != dTopLevelNode.end(); it++)
        it->second->mTraverse(0, xFtor, isGraterFnForSort);
}

bool isGraterNode(funcCallTreeNode* n1, funcCallTreeNode* n2)
{
    if(n1->mGetCount() > n2->mGetCount())
        return true;
    return false;
}

void funcCallTree::mAddToHtml(ofstream &htmlFile)
{
    htmlOutputFtor lFtor(htmlFile);
    htmlFile<<"<ul>\n";
    mTraverse(lFtor,&isGraterNode);
    htmlFile<<"</ul>\n";
}

void funcCallTree::mPrint()
{
    for(auto it = dTopLevelNode.begin(); it != dTopLevelNode.end(); it++)
        it->second->mPrint();
}


void funcCallTree::addCallStack(std::vector<string> &xQueue, ullong xCount)
{
    int lSz = xQueue.size();
    if(lSz)
    {
        std::set<funcDef*> lAlreadyDoneInThisStack; //To prevent multiple addition in case of recursive function
        funcDef *lCurrDef = funcDef::sGetFunc(xQueue[lSz-1].c_str(), "");
        funcCallTreeNode *lCurrTreeNode = dTopLevelNode[lCurrDef];
        if(!lCurrTreeNode)
        {
            lCurrTreeNode = new funcCallTreeNode(lCurrDef);
            dTopLevelNode[lCurrDef] = lCurrTreeNode;
        }
        lCurrTreeNode->mIncrCount(xCount, true);
        lAlreadyDoneInThisStack.insert(lCurrDef);
        for(int i = lSz-2; i >=0; i--)
        {
            lCurrDef = funcDef::sGetFunc(xQueue[i].c_str(), "");  
            bool lIncrDefCnt = (lAlreadyDoneInThisStack.find(lCurrDef) == lAlreadyDoneInThisStack.end()); 
            //if(!lIncrDefCnt) std::cout<<"...."<<lCurrDef->mGetName()<<std::endl;
            lAlreadyDoneInThisStack.insert(lCurrDef);
            lCurrTreeNode = lCurrTreeNode->mGetChildByDef(lCurrDef);
            lCurrTreeNode->mIncrCount(xCount, lIncrDefCnt);
        }
    }
}

void writeHtmlHeader(ofstream &out_file)
{
    out_file<<htmlHeader;
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


static void* (*real_malloc)(size_t) = NULL;
static void (*real_free)(void*) = NULL;

ullong total_alloc = 0;
ullong total_free  = 0;
funcCallTree callTree;

bool doNotLogMallocFree = false;
bool exitedOnce = false;
void exitFn()
{
    return;
}

static ullong cache_count = 0;
static ullong cache_miss = 0;
funcCallTree::~funcCallTree()
{
    doNotLogMallocFree = true;
    postTreeCreateProcess(*this);
    ofstream outFile;
    char fileName[1000];
    const char *lOutFile = getenv("MEM_PROF_OUTPUT");
    if(!lOutFile || *lOutFile == '\0') lOutFile = "mem_profile";
    sprintf(fileName, "%s_%d.html", lOutFile, getpid());
    outFile.open(fileName, ios::out);
    writeHtmlHeader(outFile);
    this->mAddToHtml(outFile);
    writeHtmlFooter(outFile);
    outFile.close();
    fprintf(stderr, "Memory profile output at: %s\n", fileName);
}

void  init() 
{
    real_malloc = (void*(*)(size_t))dlsym(RTLD_NEXT, TOSTRING(MALLOC_FUNC_NAME));
    real_free   = (void(*)(void*))dlsym(RTLD_NEXT, TOSTRING(FREE_FUNC_NAME));
    if(!real_malloc || !real_free)
    {
        fprintf(stderr, "No real malloc/free\n");
        exit(1);
    }
    //atexit(exitFn);
}

static memorySz lThresholdMem = 0_KB;
long getMemThreshold()
{
    if(lThresholdMem.dBytes == 0)
    {
        if(getenv("MEM_PROF_THRESHOLD"))
            lThresholdMem.dBytes = atol(getenv("MEM_PROF_THRESHOLD"));
        else
            lThresholdMem = 1_MB;
    }
    return lThresholdMem.dBytes;
}

void setMemThreshold(ullong xBytes)
{
    lThresholdMem.dBytes = xBytes;
}

static bool globalCtorCalled = 0;
class globalCtorCalledHelper{
    public:
        globalCtorCalledHelper(){
            globalCtorCalled = 1;
        }
};
globalCtorCalledHelper abc_g;
static std::map<int, long> thread_vs_block_size;
static unordered_map<void*, size_t> pointer_vs_size;
extern "C" {

    void glibc_override_init_hook()
    {
    }
void* MALLOC_FUNC_NAME(size_t xSize)
{
    if(!real_malloc)
        init();
    void *lPtr = real_malloc(xSize);
    int thread_id = gettid();
    if(!doNotLogMallocFree && globalCtorCalled)
    {
        //Own DS to save status
        setValueAndRestoreOnExit(doNotLogMallocFree, true);
        pointer_vs_size[lPtr] = xSize;
        long &block_size = thread_vs_block_size[thread_id];
        block_size += xSize;
        if(block_size >= getMemThreshold())
        {
            std::vector<string> lVec;
            traceFunction(lVec);
            static bool noPrint = true;
            if(noPrint)
            {
                for(int i = 0; i < lVec.size(); ++i)
                {
                    if(strcmp(lVec[i].c_str(), "main") == 0)
                        noPrint = false;
                }
            }
            if(!noPrint)
            {
                callTree.addCallStack(lVec, block_size);
                total_alloc += block_size;        
            }
            block_size = 0;
            pointer_vs_size.clear();
        }
    }
    return lPtr;
}

void FREE_FUNC_NAME(void *xPtr)
{
    if(!real_free)
        init();
    if(!doNotLogMallocFree && globalCtorCalled && xPtr)
    {
        setValueAndRestoreOnExit(doNotLogMallocFree, true);
        long &block_size = thread_vs_block_size[gettid()];
        block_size -= pointer_vs_size[xPtr];  
        pointer_vs_size[xPtr] = 0;
    }
    real_free(xPtr);
}
}
