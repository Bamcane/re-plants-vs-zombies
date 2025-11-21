#include <unistd.h>
#include "Common.h"
#include "PakInterface.h"
#include "fcaseopen/fcaseopen.h"
#include <zstd.h>

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned long ulong;

enum {
    FILEFLAGS_END = 0x80
};

PakInterface* gPakInterface = new PakInterface();

static std::string StringToUpper(const std::string& theString)
{
    std::string aString;
    for (unsigned i = 0; i < theString.length(); i++)
        aString += toupper(theString[i]);
    return aString;
}

PakInterface::PakInterface()
{
    //if (GetPakPtr() == NULL)
    //*gPakInterfaceP = this;
}

PakInterface::~PakInterface()
{
}

//0x5D84D0
static void FixFileName(const char* theFileName, char* theUpperName)
{
    // 检测路径是否为从盘符开始的绝对路径
    if ((theFileName[0] != 0) && (theFileName[1] == ':'))
    {
        char aDir[256];
        getcwd(aDir, 256); // 取得当前工作路径
        int aLen = strlen(aDir);
        aDir[aLen++] = '/';
        aDir[aLen] = 0;

        // 判断 theFileName 文件是否位于当前目录下
        if (strncasecmp(aDir, theFileName, aLen) == 0)
            theFileName += aLen; // 若是，则跳过从盘符到当前目录的部分，转化为相对路径
    }

    bool lastSlash = false;
    const char* aSrc = theFileName;
    char* aDest = theUpperName;
    for (;;)
    {
        char c = *(aSrc++);
        if ((c == '\\') || (c == '/'))
        {
            // 统一转为右斜杠，且多个斜杠的情况下只保留一个
            if (!lastSlash)
                *(aDest++) = '/';
            lastSlash = true;
        }
        else if ((c == '.') && (lastSlash) && (*aSrc == '.'))
        {
            // We have a '/..' on our hands
            aDest--;
            while ((aDest > theUpperName + 1) && (*(aDest-1) != '/')) // 回退到上一层目录
                --aDest;
            aSrc++; // 跳过下一个 '.'
            // 此处将形如“a/b/../c”的路径简化为“a/c”
        }
        else
        {
            *(aDest++) = toupper((uchar) c);
            if (c == 0)
                break;
            lastSlash = false;
        }
    }
}

bool PakInterface::AddPakFile(const std::string& theFileName)
{
	/*
	HANDLE aFileHandle = CreateFile(theFileName.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if (aFileHandle == INVALID_HANDLE_VALUE)
		return false;

	int aFileSize = GetFileSize(aFileHandle, 0);

	HANDLE aFileMapping = CreateFileMapping(aFileHandle, NULL, PAGE_READONLY, 0, aFileSize, NULL);
	if (aFileMapping == NULL)
	{
		CloseHandle(aFileHandle);
		return false;
	}

	void* aPtr = MapViewOfFile(aFileMapping, FILE_MAP_READ, 0, 0, aFileSize);
	if (aPtr == NULL)
	{
		CloseHandle(aFileMapping);
		CloseHandle(aFileHandle);
		return false;
	}
	*/

	FILE *aFileHandle = fcaseopen(theFileName.c_str(), "rb");
    if (!aFileHandle) return false;

    fseek(aFileHandle, 0, SEEK_END);
    size_t aFileSize = ftell(aFileHandle);
    fseek(aFileHandle, 0, SEEK_SET);

	mPakCollectionList.emplace_back(aFileSize);
	PakCollection* aPakCollection = &mPakCollectionList.back();
	/*
	aPakCollection->mFileHandle = aFileHandle;
	aPakCollection->mMappingHandle = aFileMapping;
	aPakCollection->mDataPtr = aPtr;
	*/

	if (fread(aPakCollection->mDataPtr, 1, aFileSize, aFileHandle) != aFileSize) {
        fclose(aFileHandle);
        return false;
    }
    fclose(aFileHandle);

    {
        auto *aDataPtr = static_cast<uint8_t *>(aPakCollection->mDataPtr);
        for (size_t i = 0; i < aFileSize; i++)
            *aDataPtr++ ^= 0xF7;
    }

	PakRecordMap::iterator aRecordItr = mPakRecordMap.insert(PakRecordMap::value_type(StringToUpper(theFileName), PakRecord())).first;
	PakRecord* aPakRecord = &(aRecordItr->second);
	aPakRecord->mCollection = aPakCollection;
	aPakRecord->mFileName = theFileName;
	aPakRecord->mStartPos = 0;
	aPakRecord->mSize = aFileSize;
	aPakRecord->mCompressedSize = aFileSize;
    aPakRecord->mFlags = 0;
	
	PFILE* aFP = FOpen(theFileName.c_str(), "rb");
	if (aFP == NULL)
		return false;

	uint32_t aMagic = 0;
	FRead(&aMagic, sizeof(uint32_t), 1, aFP);
	if (aMagic != 0xBAC04AC0)
	{
		FClose(aFP);
		return false;
	}

	uint32_t aVersion = 0;
	FRead(&aVersion, sizeof(uint32_t), 1, aFP);
	if (aVersion != 1)
	{
		FClose(aFP);
		return false;
	}

	int aPos = 0;

	for (;;)
	{
		uchar aFlags = 0;
		int aCount = FRead(&aFlags, 1, 1, aFP);
		if ((aFlags & FILEFLAGS_END) || (aCount == 0))
			break;

		uchar aNameWidth = 0;
		char aName[256];
		FRead(&aNameWidth, 1, 1, aFP);
		FRead(aName, 1, aNameWidth, aFP);
		aName[aNameWidth] = 0;
		int aSrcSize = 0;
		FRead(&aSrcSize, sizeof(int), 1, aFP);
		int aStoredSize = 0;
		FRead(&aStoredSize, sizeof(int), 1, aFP);
		int64_t aFileTime;
		FRead(&aFileTime, sizeof(int64_t), 1, aFP);

		for (int i=0; i<aNameWidth; i++)
		{
			if (aName[i] == '\\')
				aName[i] = '/'; // lol
		}

		char anUpperName[256];
		FixFileName(aName, anUpperName);

		PakRecordMap::iterator aRecordItr = mPakRecordMap.insert(PakRecordMap::value_type(StringToUpper(aName), PakRecord())).first;
		PakRecord* aPakRecord = &(aRecordItr->second);
		aPakRecord->mCollection = aPakCollection;
		aPakRecord->mFileName = anUpperName;
		aPakRecord->mStartPos = aPos;
		aPakRecord->mSize = aSrcSize;
		aPakRecord->mFileTime = aFileTime;
        aPakRecord->mCompressedSize = aStoredSize;
        aPakRecord->mFlags = aFlags;

		aPos += aStoredSize;
	}

	int anOffset = FTell(aFP);

	// Now fix file starts
	aRecordItr = mPakRecordMap.begin();
	while (aRecordItr != mPakRecordMap.end())
	{
		PakRecord* aPakRecord = &(aRecordItr->second);
		if (aPakRecord->mCollection == aPakCollection)
			aPakRecord->mStartPos += anOffset;
		++aRecordItr;
	}

	FClose(aFP);

	return true;
}

//0x5D85C0
PFILE* PakInterface::FOpen(const char* theFileName, const char* anAccess)
{
    if ((strcasecmp(anAccess, "r") == 0) || (strcasecmp(anAccess, "rb") == 0) || (strcasecmp(anAccess, "rt") == 0))
    {
        char anUpperName[256];
        FixFileName(theFileName, anUpperName);

        PakRecordMap::iterator anItr = mPakRecordMap.find(anUpperName);
        if (anItr != mPakRecordMap.end())
        {
            PFILE* aPFP = new PFILE;
            aPFP->mRecord = &anItr->second;
            aPFP->mPos = 0;
            aPFP->mFP = NULL;
            aPFP->mDecompressedCache = nullptr; // 初始化缓存指针
            return aPFP;
        }

        // 兼容非大写 key（虽然理论上不需要）
        anItr = mPakRecordMap.find(theFileName);
        if (anItr != mPakRecordMap.end())
        {
            PFILE* aPFP = new PFILE;
            aPFP->mRecord = &anItr->second;
            aPFP->mPos = 0;
            aPFP->mFP = NULL;
            aPFP->mDecompressedCache = nullptr;
            return aPFP;
        }
    }

    FILE* aFP = fcaseopen(theFileName, anAccess);
    if (aFP == NULL)
        return NULL;

    PFILE* aPFP = new PFILE;
    aPFP->mRecord = NULL;
    aPFP->mPos = 0;
    aPFP->mFP = aFP;
    aPFP->mDecompressedCache = nullptr; // 初始化
    return aPFP;
}

//0x5D8780
int PakInterface::FClose(PFILE* theFile)
{
    if (theFile->mRecord == NULL) {
        fclose(theFile->mFP);
    } else {
        if (theFile->mDecompressedCache) {
            free(theFile->mDecompressedCache);
            theFile->mDecompressedCache = nullptr;
        }
    }
    delete theFile;
    return 0;
}

//0x5D87B0
int PakInterface::FSeek(PFILE* theFile, long theOffset, int theOrigin)
{
    if (theFile->mRecord != NULL)
    {
        if (theOrigin == SEEK_SET)
            theFile->mPos = theOffset;
        else if (theOrigin == SEEK_END)
            theFile->mPos = theFile->mRecord->mSize - theOffset;
        else if (theOrigin == SEEK_CUR)
            theFile->mPos += theOffset;

        theFile->mPos = std::max(std::min(theFile->mPos, theFile->mRecord->mSize), 0);
        return 0;
    }
    else
        return fseek(theFile->mFP, theOffset, theOrigin);
}

//0x5D8830
int PakInterface::FTell(PFILE* theFile)
{
    if (theFile->mRecord != NULL)
        return theFile->mPos;
    else
        return ftell(theFile->mFP);
}

//0x5D8850
size_t PakInterface::FRead(void* thePtr, int theElemSize, int theCount, PFILE* theFile)
{
    if (!thePtr || theElemSize <= 0 || theCount <= 0 || !theFile)
        return 0;

    if (theFile->mRecord != nullptr)
    {
        PakRecord* rec = theFile->mRecord;

        // 安全检查：防止无效大小
        if (rec->mSize < 0 || rec->mCompressedSize < 0) {
            return 0;
        }
        if (static_cast<size_t>(rec->mSize) > 100 * 1024 * 1024) { // 100MB limit
            return 0;
        }

        if (theFile->mPos >= rec->mSize)
            return 0; // EOF

        if (theFile->mDecompressedCache == nullptr) {
            size_t allocSize = static_cast<size_t>(rec->mSize);
            theFile->mDecompressedCache = malloc(allocSize);
            if (!theFile->mDecompressedCache) {
                return 0; // 内存不足
            }

            void* src = static_cast<char*>(rec->mCollection->mDataPtr) + rec->mStartPos;

            if (rec->mFlags & 0x01) {
                // 压缩
                size_t result = ZSTD_decompress(
                    theFile->mDecompressedCache, allocSize,
                    src, static_cast<size_t>(rec->mCompressedSize)
                );
                if (ZSTD_isError(result) || result != allocSize) {
                    free(theFile->mDecompressedCache);
                    theFile->mDecompressedCache = nullptr;
                    return 0;
                }
            } else {
                // 未压缩
                if (rec->mCompressedSize != rec->mSize) {
                    free(theFile->mDecompressedCache);
                    theFile->mDecompressedCache = nullptr;
                    return 0;
                }
                memcpy(theFile->mDecompressedCache, src, allocSize);
            }
        }

        size_t remaining = static_cast<size_t>(rec->mSize - theFile->mPos);
        size_t requestBytes = static_cast<size_t>(theElemSize) * static_cast<size_t>(theCount);
        size_t toRead = (requestBytes < remaining) ? requestBytes : remaining;

        if (toRead > 0) {
            if (!theFile->mDecompressedCache) return 0; // 双重保险
            memcpy(thePtr, static_cast<char*>(theFile->mDecompressedCache) + theFile->mPos, toRead);
            theFile->mPos += toRead;
        }

        return toRead / static_cast<size_t>(theElemSize);
    }

    return fread(thePtr, theElemSize, theCount, theFile->mFP);
}

int PakInterface::FGetC(PFILE* theFile)
{
    if (theFile->mRecord != nullptr) {
        if (theFile->mDecompressedCache == nullptr) {
            // 尝试读取 1 字节来触发 FRead 的缓存机制
            char dummy;
            if (FRead(&dummy, 1, 1, theFile) == 0) {
                return EOF; // 解压失败或文件为空
            }
            theFile->mPos = 0;
        }

        if (theFile->mPos >= theFile->mRecord->mSize) {
            return EOF;
        }

        unsigned char c = static_cast<unsigned char*>(
            theFile->mDecompressedCache)[theFile->mPos++];

        if (c == '\r') {
            if (theFile->mPos >= theFile->mRecord->mSize) {
                return EOF;
            }
            c = static_cast<unsigned char*>(
                theFile->mDecompressedCache)[theFile->mPos++];
        }
        return static_cast<int>(c);
    }
    return fgetc(theFile->mFP);
}

int PakInterface::UnGetC(int theChar, PFILE* theFile)
{
    if (theFile->mRecord != nullptr) {
        if (theFile->mDecompressedCache == nullptr) {
            char dummy;
            if (FRead(&dummy, 1, 1, theFile) == 0) {
                return EOF;
            }
            theFile->mPos = 0;
        }

        if (theFile->mPos > 0) {
            --theFile->mPos;
        }
        return theChar;
    }
    return ungetc(theChar, theFile->mFP);
}

char* PakInterface::FGetS(char* thePtr, int theSize, PFILE* theFile)
{
    if (theFile->mRecord != nullptr) {
        if (theFile->mDecompressedCache == nullptr) {
            char dummy;
            if (FRead(&dummy, 1, 1, theFile) == 0) {
                return nullptr; // 无法加载内容
            }
            theFile->mPos = 0;
        }

        if (theFile->mPos >= theFile->mRecord->mSize) {
            return nullptr; // EOF
        }

        int idx = 0;
        while (idx < theSize - 1) { // 留一个位置给 '\0'
            if (theFile->mPos >= theFile->mRecord->mSize) {
                break;
            }

            unsigned char c = static_cast<unsigned char*>(
                theFile->mDecompressedCache)[theFile->mPos++];

            if (c == '\r') {
                if (theFile->mPos < theFile->mRecord->mSize) {
                    unsigned char next = static_cast<unsigned char*>(
                        theFile->mDecompressedCache)[theFile->mPos];
                    if (next == '\n') {
                        c = '\n';
                        ++theFile->mPos;
                    } else {
                        continue;
                    }
                } else {
                    // \r at end, treat as newline?
                    c = '\n';
                }
            }

            thePtr[idx++] = static_cast<char>(c);

            if (c == '\n') {
                break;
            }
        }

        thePtr[idx] = '\0';
        return thePtr;
    }
    return fgets(thePtr, theSize, theFile->mFP);
}

int PakInterface::FEof(PFILE* theFile)
{
    if (theFile->mRecord != NULL)
        return theFile->mPos >= theFile->mRecord->mSize;
    else
        return feof(theFile->mFP);
}