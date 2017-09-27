#pragma once

#include <Windows.h> 
#include <TlHelp32.h> 

enum
{
	HOOK_IAT=1,
	HOOK_JMP,
	HOOK_MHOOK
};

typedef struct tag_HOOKAPI
{
	LPCSTR szFuncName;// �� HOOK �� API �������ơ�
	LPCSTR szDllName;

	PROC pNewProc;// ���������ַ��
	PROC pOldProc;// ԭ API ������ַ��

	unsigned char oldFirstCode[8];
	unsigned char newFirstCode[8];
	unsigned int codeLen;

	int type;
	bool result; //is hook success
}HOOKAPI, *LPHOOKAPI;

bool HookAPI(HMODULE hModule, LPHOOKAPI pHookApi);
bool UnHookAPI(HMODULE hModule, LPHOOKAPI pHookApi);
bool HookAPIRecovery(LPHOOKAPI pHookApi);
bool HookAPIResume(LPHOOKAPI pHookApi);
