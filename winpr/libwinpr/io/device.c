/**
 * WinPR: Windows Portable Runtime
 * Asynchronous I/O Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <winpr/config.h>

#include <winpr/io.h>

#ifndef _WIN32

#include "io.h"

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>

#ifdef WINPR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/file.h>

/**
 * I/O Manager Routines
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff551797/
 *
 * These routines are only accessible to kernel drivers, but we need
 * similar functionality in WinPR in user space.
 *
 * This is a best effort non-conflicting port of this API meant for
 * non-Windows, WinPR usage only.
 *
 * References:
 *
 * Device Objects and Device Stacks:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff543153/
 *
 * Driver Development Part 1: Introduction to Drivers:
 * http://www.codeproject.com/Articles/9504/Driver-Development-Part-1-Introduction-to-Drivers/
 */

#define DEVICE_FILE_PREFIX_PATH "\\Device\\"

static char* GetDeviceFileNameWithoutPrefixA(LPCSTR lpName)
{
	char* lpFileName = NULL;

	if (!lpName)
		return NULL;

	if (strncmp(lpName, DEVICE_FILE_PREFIX_PATH, sizeof(DEVICE_FILE_PREFIX_PATH) - 1) != 0)
		return NULL;

	lpFileName =
	    _strdup(&lpName[strnlen(DEVICE_FILE_PREFIX_PATH, sizeof(DEVICE_FILE_PREFIX_PATH))]);
	return lpFileName;
}

static char* GetDeviceFileUnixDomainSocketBaseFilePathA(void)
{
	char* lpTempPath = NULL;
	char* lpPipePath = NULL;
	lpTempPath = GetKnownPath(KNOWN_PATH_TEMP);

	if (!lpTempPath)
		return NULL;

	lpPipePath = GetCombinedPath(lpTempPath, ".device");
	free(lpTempPath);
	return lpPipePath;
}

static char* GetDeviceFileUnixDomainSocketFilePathA(LPCSTR lpName)
{
	char* lpPipePath = NULL;
	char* lpFileName = NULL;
	char* lpFilePath = NULL;
	lpPipePath = GetDeviceFileUnixDomainSocketBaseFilePathA();

	if (!lpPipePath)
		return NULL;

	lpFileName = GetDeviceFileNameWithoutPrefixA(lpName);

	if (!lpFileName)
	{
		free(lpPipePath);
		return NULL;
	}

	lpFilePath = GetCombinedPath(lpPipePath, lpFileName);
	free(lpPipePath);
	free(lpFileName);
	return lpFilePath;
}

/**
 * IoCreateDevice:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff548397/
 */

NTSTATUS _IoCreateDeviceEx(WINPR_ATTR_UNUSED PDRIVER_OBJECT_EX DriverObject,
                           WINPR_ATTR_UNUSED ULONG DeviceExtensionSize, PUNICODE_STRING DeviceName,
                           WINPR_ATTR_UNUSED DEVICE_TYPE DeviceType,
                           WINPR_ATTR_UNUSED ULONG DeviceCharacteristics,
                           WINPR_ATTR_UNUSED BOOLEAN Exclusive, PDEVICE_OBJECT_EX* DeviceObject)
{
	int status = 0;
	char* DeviceBasePath = NULL;
	DEVICE_OBJECT_EX* pDeviceObjectEx = NULL;
	DeviceBasePath = GetDeviceFileUnixDomainSocketBaseFilePathA();

	if (!DeviceBasePath)
		return STATUS_NO_MEMORY;

	if (!winpr_PathFileExists(DeviceBasePath))
	{
		if (mkdir(DeviceBasePath, S_IRUSR | S_IWUSR | S_IXUSR) != 0)
		{
			free(DeviceBasePath);
			return STATUS_ACCESS_DENIED;
		}
	}

	free(DeviceBasePath);
	pDeviceObjectEx = (DEVICE_OBJECT_EX*)calloc(1, sizeof(DEVICE_OBJECT_EX));

	if (!pDeviceObjectEx)
		return STATUS_NO_MEMORY;

	pDeviceObjectEx->DeviceName =
	    ConvertWCharNToUtf8Alloc(DeviceName->Buffer, DeviceName->Length / sizeof(WCHAR), NULL);
	if (!pDeviceObjectEx->DeviceName)
	{
		free(pDeviceObjectEx);
		return STATUS_NO_MEMORY;
	}

	pDeviceObjectEx->DeviceFileName =
	    GetDeviceFileUnixDomainSocketFilePathA(pDeviceObjectEx->DeviceName);

	if (!pDeviceObjectEx->DeviceFileName)
	{
		free(pDeviceObjectEx->DeviceName);
		free(pDeviceObjectEx);
		return STATUS_NO_MEMORY;
	}

	if (winpr_PathFileExists(pDeviceObjectEx->DeviceFileName))
	{
		if (unlink(pDeviceObjectEx->DeviceFileName) == -1)
		{
			free(pDeviceObjectEx->DeviceName);
			free(pDeviceObjectEx->DeviceFileName);
			free(pDeviceObjectEx);
			return STATUS_ACCESS_DENIED;
		}
	}

	status = mkfifo(pDeviceObjectEx->DeviceFileName, 0666);

	if (status != 0)
	{
		free(pDeviceObjectEx->DeviceName);
		free(pDeviceObjectEx->DeviceFileName);
		free(pDeviceObjectEx);

		switch (errno)
		{
			case EACCES:
				return STATUS_ACCESS_DENIED;

			case EEXIST:
				return STATUS_OBJECT_NAME_EXISTS;

			case ENAMETOOLONG:
				return STATUS_NAME_TOO_LONG;

			case ENOENT:
			case ENOTDIR:
				return STATUS_NOT_A_DIRECTORY;

			case ENOSPC:
				return STATUS_DISK_FULL;

			default:
				return STATUS_INTERNAL_ERROR;
		}
	}

	*((ULONG_PTR*)(DeviceObject)) = (ULONG_PTR)pDeviceObjectEx;
	return STATUS_SUCCESS;
}

/**
 * IoDeleteDevice:
 * http://msdn.microsoft.com/en-us/library/windows/hardware/ff549083/
 */

VOID _IoDeleteDeviceEx(PDEVICE_OBJECT_EX DeviceObject)
{
	DEVICE_OBJECT_EX* pDeviceObjectEx = NULL;
	pDeviceObjectEx = (DEVICE_OBJECT_EX*)DeviceObject;

	if (!pDeviceObjectEx)
		return;

	unlink(pDeviceObjectEx->DeviceFileName);
	free(pDeviceObjectEx->DeviceName);
	free(pDeviceObjectEx->DeviceFileName);
	free(pDeviceObjectEx);
}

#endif
