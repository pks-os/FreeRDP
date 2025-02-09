
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>

static TCHAR testDirectory2File1[] = _T("TestDirectory2File1");
static TCHAR testDirectory2File2[] = _T("TestDirectory2File2");

int TestFileFindNextFile(int argc, char* argv[])
{
	char* str = NULL;
	size_t length = 0;
	BOOL status = 0;
	HANDLE hFind = NULL;
	LPTSTR BasePath = NULL;
	WIN32_FIND_DATA FindData;
	TCHAR FilePath[PATHCCH_MAX_CCH] = { 0 };
	WINPR_UNUSED(argc);
	str = argv[1];
#ifdef UNICODE
	BasePath = ConvertUtf8ToWChar(str, &length);

	if (!BasePath)
	{
		_tprintf(_T("Unable to allocate memory"));
		return -1;
	}
#else
	BasePath = _strdup(str);

	if (!BasePath)
	{
		printf("Unable to allocate memory");
		return -1;
	}

	length = strlen(BasePath);
#endif
	/* Simple filter matching all files inside current directory */
	CopyMemory(FilePath, BasePath, length * sizeof(TCHAR));
	FilePath[length] = 0;
	PathCchConvertStyle(BasePath, length, PATH_STYLE_WINDOWS);
	NativePathCchAppend(FilePath, PATHCCH_MAX_CCH, _T("TestDirectory2"));
	NativePathCchAppend(FilePath, PATHCCH_MAX_CCH, _T("TestDirectory2File*"));
	free(BasePath);
	_tprintf(_T("Finding file: %s\n"), FilePath);
	hFind = FindFirstFile(FilePath, &FindData);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		_tprintf(_T("FindFirstFile failure: %s\n"), FilePath);
		return -1;
	}

	_tprintf(_T("FindFirstFile: %s"), FindData.cFileName);

	/**
	 * The current implementation does not enforce a particular order
	 */

	if ((_tcsncmp(FindData.cFileName, testDirectory2File1, ARRAYSIZE(testDirectory2File1)) != 0) &&
	    (_tcsncmp(FindData.cFileName, testDirectory2File2, ARRAYSIZE(testDirectory2File2)) != 0))
	{
		_tprintf(_T("FindFirstFile failure: Expected: %s, Actual: %s\n"), testDirectory2File1,
		         FindData.cFileName);
		return -1;
	}

	status = FindNextFile(hFind, &FindData);

	if (!status)
	{
		_tprintf(_T("FindNextFile failure: Expected: TRUE, Actual: %") _T(PRId32) _T("\n"), status);
		return -1;
	}

	if ((_tcsncmp(FindData.cFileName, testDirectory2File1, ARRAYSIZE(testDirectory2File1)) != 0) &&
	    (_tcsncmp(FindData.cFileName, testDirectory2File2, ARRAYSIZE(testDirectory2File2)) != 0))
	{
		_tprintf(_T("FindNextFile failure: Expected: %s, Actual: %s\n"), testDirectory2File2,
		         FindData.cFileName);
		return -1;
	}

	status = FindNextFile(hFind, &FindData);

	if (status)
	{
		_tprintf(_T("FindNextFile failure: Expected: FALSE, Actual: %") _T(PRId32) _T("\n"),
		         status);
		return -1;
	}

	FindClose(hFind);
	return 0;
}
