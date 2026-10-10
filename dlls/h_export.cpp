/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== h_export.cpp ========================================================

  Entity classes exported by Halflife.

*/

#include "extdll.h"
#include "util.h"

#include "cbase.h"

#ifdef _WIN32
#include <DbgHelp.h>
#include <stdio.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

// Holds engine functionality callbacks
enginefuncs_t g_engfuncs;
globalvars_t  *gpGlobals;

#undef DLLEXPORT
#ifdef _WIN32
#define DLLEXPORT __stdcall
#else
#define DLLEXPORT __attribute__ ((visibility("default")))
#endif

#ifdef _WIN32

static LPTOP_LEVEL_EXCEPTION_FILTER g_prevUnhandledExceptionFilter = NULL;
static volatile LONG g_crashFilterActive = 0;
static char g_crashBreadcrumb[256] = "dll loaded";

static void CI_SetCrashBreadcrumb(const char* breadcrumb)
{
  if (!breadcrumb)
  {
    return;
  }

  strncpy_s(g_crashBreadcrumb, sizeof(g_crashBreadcrumb), breadcrumb, _TRUNCATE);
}

static void CI_WriteCrashLog(const char* logPath, EXCEPTION_POINTERS* exceptionInfo)
{
  SYSTEMTIME now;
  GetLocalTime(&now);

  FILE* logFile = NULL;
  errno_t openResult = fopen_s(&logFile, logPath, "w");
  if (openResult != 0 || !logFile)
  {
    return;
  }

  fprintf(logFile, "Cold Ice Remastered crash report\n");
  fprintf(logFile, "Timestamp: %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
    now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
  fprintf(logFile, "ProcessId: %lu\n", (unsigned long)GetCurrentProcessId());
  fprintf(logFile, "ThreadId: %lu\n", (unsigned long)GetCurrentThreadId());
  fprintf(logFile, "Breadcrumb: %s\n", g_crashBreadcrumb);

  if (exceptionInfo && exceptionInfo->ExceptionRecord)
  {
    const EXCEPTION_RECORD* record = exceptionInfo->ExceptionRecord;
    fprintf(logFile, "ExceptionCode: 0x%08lX\n", (unsigned long)record->ExceptionCode);
    fprintf(logFile, "ExceptionFlags: 0x%08lX\n", (unsigned long)record->ExceptionFlags);
    fprintf(logFile, "ExceptionAddress: 0x%p\n", record->ExceptionAddress);
  }

  fclose(logFile);
}

static void CI_WriteMiniDump(const char* dumpPath, EXCEPTION_POINTERS* exceptionInfo)
{
  HANDLE dumpFile = CreateFileA(
    dumpPath,
    GENERIC_WRITE,
    0,
    NULL,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL);

  if (dumpFile == INVALID_HANDLE_VALUE)
  {
    return;
  }

  MINIDUMP_EXCEPTION_INFORMATION miniDumpExceptionInfo;
  miniDumpExceptionInfo.ThreadId = GetCurrentThreadId();
  miniDumpExceptionInfo.ExceptionPointers = exceptionInfo;
  miniDumpExceptionInfo.ClientPointers = FALSE;

  MiniDumpWriteDump(
    GetCurrentProcess(),
    GetCurrentProcessId(),
    dumpFile,
    (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
    exceptionInfo ? &miniDumpExceptionInfo : NULL,
    NULL,
    NULL);

  CloseHandle(dumpFile);
}

static LONG WINAPI CI_UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
  if (InterlockedCompareExchange(&g_crashFilterActive, 1, 0) != 0)
  {
    return EXCEPTION_EXECUTE_HANDLER;
  }

  CreateDirectoryA("crash_dumps", NULL);

  SYSTEMTIME now;
  GetLocalTime(&now);

  char dumpPath[MAX_PATH];
  char logPath[MAX_PATH];
  snprintf(
    dumpPath,
    sizeof(dumpPath),
    "crash_dumps\\ice_%04u%02u%02u_%02u%02u%02u_%lu.dmp",
    now.wYear,
    now.wMonth,
    now.wDay,
    now.wHour,
    now.wMinute,
    now.wSecond,
    (unsigned long)GetCurrentProcessId());

  snprintf(
    logPath,
    sizeof(logPath),
    "crash_dumps\\ice_%04u%02u%02u_%02u%02u%02u_%lu.log",
    now.wYear,
    now.wMonth,
    now.wDay,
    now.wHour,
    now.wMinute,
    now.wSecond,
    (unsigned long)GetCurrentProcessId());

  CI_WriteMiniDump(dumpPath, exceptionInfo);
  CI_WriteCrashLog(logPath, exceptionInfo);

  InterlockedExchange(&g_crashFilterActive, 0);
  return EXCEPTION_EXECUTE_HANDLER;
}

// Required DLL entry point
BOOL WINAPI DllMain(
   HINSTANCE hinstDLL,
   DWORD fdwReason,
   LPVOID lpvReserved)
{
	if      (fdwReason == DLL_PROCESS_ATTACH)
    {
    CI_SetCrashBreadcrumb("dll process attach");
    g_prevUnhandledExceptionFilter = SetUnhandledExceptionFilter(CI_UnhandledExceptionFilter);
    }
	else if (fdwReason == DLL_PROCESS_DETACH)
    {
    CI_SetCrashBreadcrumb("dll process detach");
    SetUnhandledExceptionFilter(g_prevUnhandledExceptionFilter);
    }
	return TRUE;
}
#endif

extern "C" void DLLEXPORT GiveFnptrsToDll(	enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals )
{
  #ifdef _WIN32
  CI_SetCrashBreadcrumb("GiveFnptrsToDll");
  #endif
	memcpy(&g_engfuncs, pengfuncsFromEngine, sizeof(enginefuncs_t));
	gpGlobals = pGlobals;
}


