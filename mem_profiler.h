#include <vector>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include "common.h"

using namespace std;
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#ifndef MALLOC_FUNC_NAME 
    #define MALLOC_FUNC_NAME malloc
#endif
#ifndef FREE_FUNC_NAME 
    #define FREE_FUNC_NAME free
#endif

#define nullptr NULL

typedef unsigned long long ullong;
int debug = 0;

class funcDef{
    private:
        string name;
        string source;
        ullong dTotalAllocBytes;

        funcDef(const char* xName, const char* xSource):name(xName),source(xSource)
        {dTotalAllocBytes = 0;}

        //Name, source
        static unordered_map<string, funcDef*> sFuncMap;
        static vector<funcDef*> sFuncList;
    public:
        static funcDef *sGetFunc(const char* name, const char* source);
        void mIncrCount(ullong xCount){dTotalAllocBytes += xCount;}
        ullong mGetTotalCount(){return dTotalAllocBytes;}
        const char *mGetName(){return name.c_str();}
        const char *mGetSrcInfo(){return source.c_str();}
        void mPrint(){cout <<name<<"("<<source<<") ";}
};

template<typename T>
class functorBase{
    public:
        virtual void operator()(int, int, T*) = 0;
};

class funcCallTreeNode;
typedef functorBase<funcCallTreeNode> tdTreeNodeFunctor;

typedef bool (*tdIsGraterFnForSort)(funcCallTreeNode*, funcCallTreeNode*);
class funcCallTreeNode{
    funcDef *dFuncDefPtr;
    unsigned long long dThisCallAllocBytes;
    unsigned long long dThisCallFreeBytes;
    std::unordered_map<funcDef*, funcCallTreeNode*> dChildMap;
    std::vector<funcCallTreeNode*> dChildList;

    public:
        funcCallTreeNode *mGetChildByName(const char *xFuncName, const char *xSrcInfo);
        funcCallTreeNode *mGetChildByDef(funcDef*);
        funcCallTreeNode *mGetChildByIndex(int);
        void mIncrCount(ullong xNumBytes, bool xAddForDef)
        {
            dThisCallAllocBytes+=xNumBytes; 
            if (xAddForDef) dFuncDefPtr->mIncrCount(xNumBytes);
        }
        ullong mGetCount() {return dThisCallAllocBytes;}
        funcDef *mGetFnDef() {return dFuncDefPtr;}
        void mPrint(int);
        void mTraverse(int, tdTreeNodeFunctor&, tdIsGraterFnForSort=nullptr);
        int mGetChildCount() {return dChildList.size();}
        funcCallTreeNode(funcDef*);
};

class htmlOutputFtor:public tdTreeNodeFunctor
{
    public:
        ofstream &file;
        htmlOutputFtor(ofstream &xFile):file(xFile){}
        void operator()(int level, int isPreCall, funcCallTreeNode*);
};

class treeNodeInFuncTimeCalc:public tdTreeNodeFunctor
{
    static std::map<funcCallTreeNode*, ullong>sFuncNodeInTime;
    public:
        void operator()(int level, int isPreCall, funcCallTreeNode*);
        static ullong sGetFuncNodeInTime(funcCallTreeNode*);
};

class funcCallTree{
    public:
        ~funcCallTree();
        map<funcDef*, funcCallTreeNode*> dTopLevelNode;
        void addCallStack(std::vector<string> &xQueue, ullong);
        void mPrint();
        void mAddToHtml(ofstream& htmlFile);
        void mTraverse(tdTreeNodeFunctor &xFtor, tdIsGraterFnForSort=nullptr);
};

