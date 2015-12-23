/**
 * @file    LogFileAppender.cpp
 * @author  Longwei Lai<lailongwei@126.com>
 * @date    2013/06/11
 * @version 1.0
 *
 * @brief
 */

#include "llbc/common/Export.h"
#include "llbc/common/BeforeIncl.h"

#include "llbc/core/os/OS_Console.h"

#include "llbc/core/time/Time.h"
#include "llbc/core/file/File.h"
#include "llbc/core/utils/Util_Text.h"
#include "llbc/core/utils/Util_Math.h"
#include "llbc/core/utils/Util_Debug.h"

#include "llbc/core/log/LogData.h"
#include "llbc/core/log/LogLevel.h"
#include "llbc/core/log/LogTokenChain.h"
#include "llbc/core/log/LogFileAppender.h"

__LLBC_INTERNAL_NS_BEGIN
const static LLBC_NS LLBC_String __ClearOpenFlag = "wb";
const static LLBC_NS LLBC_String __AppendOpenFlag = "ab";
__LLBC_INTERNAL_NS_END

__LLBC_NS_BEGIN

LLBC_LogFileAppender::LLBC_LogFileAppender()
: _baseName()

, _fileBufferSize(0)
, _isDailyRolling(true)

, _maxFileSize(LONG_MAX)
, _maxBackupIndex(INT_MAX)

, _fileName()

, _file(NULL)
, _fileSize(0)

, _nonFlushLogCount(0)
, _logfileLastCheckTime(0)
{
}

LLBC_LogFileAppender::~LLBC_LogFileAppender()
{
    this->Finalize();
}

int LLBC_LogFileAppender::GetType() const
{
    return LLBC_LogAppenderType::File;
}

int LLBC_LogFileAppender::Initialize(const LLBC_LogAppenderInitInfo &initInfo)
{
    if (_Base::Initialize(initInfo) != LLBC_RTN_OK)
        return LLBC_RTN_FAILED;

    if (initInfo.file.empty())
    {
        LLBC_SetLastError(LLBC_ERROR_ARG);
        return LLBC_RTN_FAILED;
    }

    _baseName = initInfo.file;

    _fileBufferSize = MAX(0, initInfo.fileBufferSize);
    _isDailyRolling = initInfo.dailyRolling;

    _maxFileSize = initInfo.maxFileSize > 0 ? initInfo.maxFileSize : LONG_MAX;
    _maxBackupIndex = MAX(0, initInfo.maxBackupIndex);

    const time_t now = time(NULL);
    const LLBC_String fileName = this->BuildLogFileName(now);
    if (this->ReOpenFile(fileName, false) != LLBC_RTN_OK)
        return LLBC_RTN_FAILED;

    if (_fileSize >= _maxFileSize)
    {
        this->BackupFiles();
        if (this->ReOpenFile(fileName, true) != LLBC_RTN_OK)
            return LLBC_RTN_FAILED;
    }

    _nonFlushLogCount = 0;
    _logfileLastCheckTime = now;

    return LLBC_RTN_OK;
}

void LLBC_LogFileAppender::Finalize()
{
    _baseName.clear();

    _fileBufferSize = 0;
    _isDailyRolling = false;

    _maxFileSize = LONG_MAX;
    _maxBackupIndex = INT_MAX;

    _fileName.clear();

    LLBC_XDelete(_file);
    _fileSize = 0;

    _nonFlushLogCount = 0;
    _logfileLastCheckTime = 0;

    _Base::Finalize();
}

int LLBC_LogFileAppender::Output(const LLBC_LogData &data)
{
    LLBC_LogTokenChain *chain = NULL;
    if (!(chain = this->GetTokenChain()))
    {
        LLBC_SetLastError(LLBC_ERROR_NOT_INIT);
        return LLBC_RTN_FAILED;
    }

    this->CheckAndUpdateLogFile(data.logTime);

    LLBC_String formattedData;
    chain->Format(data, formattedData);

    const size_t actuallyWrite = 
        _file->Write(formattedData.data(), formattedData.size());
    if (actuallyWrite != LLBC_File::npos)
    {
        _fileSize += actuallyWrite;
        if (_fileBufferSize > 0) // If file buffered, process flush logic
        {
            _nonFlushLogCount += 1;
            if (data.level >= LLBC_LogLevel::Warn && 
                _fileBufferSize > 0)
                this->Flush();
        }

        if (actuallyWrite != formattedData.size())
        {
            LLBC_SetLastError(LLBC_ERROR_TRUNCATED);
            return LLBC_RTN_FAILED;
        }

        return LLBC_RTN_OK;
    }

    return LLBC_RTN_FAILED;
}

void LLBC_LogFileAppender::Flush()
{
    if (_nonFlushLogCount == 0)
        return;

    if (LIKELY(_file))
    {
        _file->Flush();
        _nonFlushLogCount = 0;
    }
}

void LLBC_LogFileAppender::CheckAndUpdateLogFile(time_t now)
{
    if (_fileSize < _maxFileSize && 
        LLBC_Abs(_logfileLastCheckTime - now) == 0)
        return;

    bool clear = false, backup = false;
    const LLBC_String newFileName = this->BuildLogFileName(now);
    if (!this->IsNeedReOpenFile(now, newFileName, clear, backup))
        return;

    if (backup)
        this->BackupFiles();

    this->ReOpenFile(newFileName, clear);
    _logfileLastCheckTime = now;
}

LLBC_String LLBC_LogFileAppender::BuildLogFileName(time_t now) const
{
    const LLBC_Time llbcNow(now);

    LLBC_String logFile(_baseName);
    if (_isDailyRolling)
        logFile.append_format(".%s", llbcNow.Format("%Y-%m-%d").c_str());

    return logFile;
}

bool LLBC_LogFileAppender::IsNeedReOpenFile(time_t now,
                                            const LLBC_String &newFileName,
                                            bool &clear,
                                            bool &backup) const
{

    if (_fileSize >= _maxFileSize)
    {
        clear = true;
        backup = true;

        return true;
    }
    else if (::memcmp(newFileName.data(), _fileName.data(), _fileName.size()) != 0)
    {
        clear = false;
        backup = false;

        return true;
    }
    else if (!LLBC_File::Exist(newFileName))
    {
        clear = true;
        backup = false;

        return true;
    }

    return false;
}

int LLBC_LogFileAppender::ReOpenFile(const LLBC_String &newFileName, bool clear)
{
    const LLBC_String openMode = 
        clear ? LLBC_INL_NS __ClearOpenFlag : LLBC_INL_NS __AppendOpenFlag;

    // Close old file.
    if (_file)
        _file->Close();
    else
        _file = LLBC_New(LLBC_File);

    // Reset non-reflush log count variables.
    _nonFlushLogCount = 0;
    // Do reopen file.
    if (UNLIKELY(_file->Open(newFileName, openMode) != LLBC_RTN_OK))
    {
#ifdef LLBC_DEBUG
        traceline("LLBC_LogFileAppender::ReOpenFile(): Open file failed, name:%s, openMode:%s, reason:%s",
            __FILE__, __LINE__, newFileName.c_str(), openMode.c_str(), LLBC_FormatLastError());
#endif
        return LLBC_RTN_FAILED;
    }

    // Update file name/size, buffer info.
    _fileName = newFileName;
    _fileSize = _file->GetSize();
    this->UpdateFileBufferInfo();

    return LLBC_RTN_OK;
}

void LLBC_LogFileAppender::BackupFiles() const
{
    if (_maxBackupIndex == 0)
        return;
    else if (!LLBC_File::Exist(_fileName))
        return;

    if (_file->IsOpened())
        _file->Close();

    int availableIndex = 0;
    while (availableIndex < _maxBackupIndex)
    {
        availableIndex += 1;
        LLBC_String backupFileName;
        backupFileName.format("%s.%d", _fileName.c_str(), availableIndex);
        if (!LLBC_File::Exist(backupFileName))
            break;
    }

    for (int willMoveIndex = availableIndex - 1; willMoveIndex >= 0; willMoveIndex--)
    {
        LLBC_String willMove;
        if (willMoveIndex > 0)
            willMove.format("%s.%d", _fileName.c_str(), willMoveIndex);
        else
            willMove = _fileName;

        LLBC_String moveTo;
        moveTo.format("%s.%d", _fileName.c_str(), willMoveIndex + 1);

#ifdef LLBC_RELEASE
        LLBC_File::Move(willMove, moveTo, true);
#else
        if (LLBC_File::Move(willMove, moveTo, true) != LLBC_RTN_OK)
        {
            traceline("LLBC_LogFileAppender::BackupFiles(): Backup failed, reason: %s", LLBC_FormatLastError());
        }
#endif
    }
}

void LLBC_LogFileAppender::UpdateFileBufferInfo()
{
    if (_fileBufferSize == 0)
        _file->SetBufferMode(LLBC_FileBufferMode::NoBuf, 0);
    else
        _file->SetBufferMode(LLBC_FileBufferMode::FullBuf, _fileBufferSize);
}

__LLBC_NS_END

#include "llbc/common/AfterIncl.h"