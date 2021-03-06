// The MIT License (MIT)

// Copyright (c) 2013 lailongwei<lailongwei@126.com>
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of 
// this software and associated documentation files (the "Software"), to deal in 
// the Software without restriction, including without limitation the rights to 
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of 
// the Software, and to permit persons to whom the Software is furnished to do so, 
// subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all 
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "comm/TestCase_Comm_DynLoadFacade.h"

TestCase_Comm_DynLoadFacade::TestCase_Comm_DynLoadFacade()
: _svc(LLBC_IService::Create(LLBC_IService::Normal, "DynLoadTestSvc", NULL, true))
// : _svc(LLBC_IService::Create(LLBC_IService::Normal, "DynLoadTestSvc", NULL, false))
{
}

TestCase_Comm_DynLoadFacade::~TestCase_Comm_DynLoadFacade()
{
    LLBC_Delete(_svc);
}

int TestCase_Comm_DynLoadFacade::Run(int argc, char *argv[])
{
    LLBC_PrintLine("Communication Service Dynamic Load Facade Test:");
    LLBC_PrintLine("Note: You must be build your facade library first!");

    LLBC_String libPath, facadeName;
    LLBC_Print("Please input your facade library path:");
    std::cin >> libPath;

    LLBC_Print("Please input your facade name:");
    std::cin >> facadeName;

    LLBC_PrintLine("Will register facade in service");
    int ret = _svc->RegisterFacade(libPath, facadeName);
    if (ret != LLBC_OK)
    {
        LLBC_FilePrintLine(stderr, "Failed to register dynamic facade, error:%s", LLBC_FormatLastError());
        return LLBC_FAILED;
    }
    LLBC_PrintLine("Register dynamic facade success, try start service");
    if ((ret = _svc->Start()) != LLBC_OK)
    {
        LLBC_FilePrintLine(stderr, "Start service failed, error:%s", LLBC_FormatLastError());
        return LLBC_FAILED;
    }

    LLBC_PrintLine("Try call facade method: Foo");
    LLBC_IFacade *facade = _svc->GetFacade("TestFacade");
    LLBC_Variant arg(3);
    LLBC_Variant callRet(4);
    ret = facade->CallMethod("Foo", arg, callRet);
    if (ret != LLBC_OK)
    {
        LLBC_PrintLine("Call method Foo success, but return code not equal to 0, error(maybe incorrect):%s", LLBC_FormatLastError());
        return LLBC_FAILED;
    }
    else
    {
        LLBC_PrintLine("Call method Foo success, callRet:%s", callRet.ToString().c_str());
    }

    LLBC_PrintLine("Press any key to continue...");
    getchar();

    return 0;
}

