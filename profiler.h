#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <sstream>
#include "common.h"

using namespace std;

#define nullptr NULL

int debug = 0;
class funcDef{
    private:
        string name;
        string source;
        unsigned long dTotalCount;
        std::unordered_map<int, unsigned long> dThreadVsTotalCount;

        funcDef(const char* xName, const char* xSource):name(xName),source(xSource)
        {dTotalCount = 0;}

        //Name, source
        static unordered_map<string, unordered_map<string, funcDef*>> sFuncMap;
        static vector<funcDef*> sFuncList;
    public:
        static funcDef *sGetFunc(const char* name, const char* source);
        void mIncrCount(int tid){++dTotalCount; ++dThreadVsTotalCount[tid];}
        unsigned long mGetTotalCount(){return dTotalCount;}
        unsigned long mGetTotalCountForThread(int tid){return dThreadVsTotalCount[tid];}
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

class funcCallTreeNode{
    funcDef *dFuncDefPtr;
    int dThisCallCount;
    std::unordered_map<funcDef*, funcCallTreeNode*> dChildMap;
    std::vector<funcCallTreeNode*> dChildList;

    public:
        funcCallTreeNode *mGetChildByName(const char *xFuncName, const char *xSrcInfo);
        funcCallTreeNode *mGetChildByDef(funcDef*);
        funcCallTreeNode *mGetChildByIndex(int);
        void mIncrCount(bool xIncFuncDef = true, int tid = 0){dThisCallCount++; if(xIncFuncDef) dFuncDefPtr->mIncrCount(tid);}
        int mGetCount() {return dThisCallCount;}
        funcDef *mGetFnDef() {return dFuncDefPtr;}
        void mPrint(int);
        void mTraverse(int, tdTreeNodeFunctor&);
        int mGetChildCount() {return dChildList.size();}
        funcCallTreeNode(funcDef*);
};

class htmlOutputFtor:public tdTreeNodeFunctor
{
    public:
        int d_currThreadProcessing = 0;
        ofstream &file;
        htmlOutputFtor(ofstream &xFile):file(xFile){}
        void operator()(int level, int isPreCall, funcCallTreeNode*);
};

class treeNodeInFuncTimeCalc:public tdTreeNodeFunctor
{
    static std::unordered_map<funcCallTreeNode*, int>sFuncNodeInTime;
    public:
        void operator()(int level, int isPreCall, funcCallTreeNode*);
        static int sGetFuncNodeInTime(funcCallTreeNode*);
};

class funcCallTree{
    public:
        map<funcDef*, funcCallTreeNode*> dTopLevelNode;
        void addCallStack(std::vector<funcDef*> &xQueue, int tid = 0);
        void addCallStack(std::vector<std::string> &xQueue, int tid = 0);
        void mPrint();
        void mAddToHtml(ofstream& htmlFile);
        void mTraverse(tdTreeNodeFunctor &xFtor);
};

