#include "stdafx.h"
#include <stdio.h> 
#include <Windows.h> 
#include <TlHelp32.h> 
#include <io.h>
#include "entry.h"
#include "hookapi.h"
#include "mhook-lib/mhook.h"

static PIMAGE_IMPORT_DESCRIPTOR LocationIAT(HMODULE hModule, LPCSTR szImportMod)
// ���У� hModule Ϊ����ģ������ szImportMod Ϊ��������ơ�
{
	// ����Ƿ�Ϊ DOS �������Ƿ��� NULL ���� DOS ����û�� IAT ��
	PIMAGE_DOS_HEADER pDOSHeader = (PIMAGE_DOS_HEADER)hModule;
	if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE) 
		return NULL;

	// ����Ƿ�Ϊ NT ��־�����򷵻� NULL ��
	PIMAGE_NT_HEADERS pNTHeader = (PIMAGE_NT_HEADERS)((DWORD)pDOSHeader + (DWORD)(pDOSHeader->e_lfanew));
	if (pNTHeader->Signature != IMAGE_NT_SIGNATURE) 
		return NULL;

	// û�� IAT ���򷵻� NULL ��
	if (pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress == 0) 
		return NULL;

	// ��λ��һ�� IAT λ�á�
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)pDOSHeader + (DWORD)(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));
	// �������������ѭ��������е� IAT ����ƥ���򷵻ظ� IAT ��ַ����������һ�� IAT ��
	while (pImportDesc->Name)
	{
		// ��ȡ�� IAT ��������������ơ�
		PSTR szCurrMod = (PSTR)((DWORD)pDOSHeader + (DWORD)(pImportDesc->Name));
		if (_stricmp(szCurrMod, szImportMod) == 0) 
			break;

		pImportDesc++;
	}

	if (pImportDesc->Name == NULL) 
		return NULL;

	return pImportDesc;
}

static bool UnHookAPIByIAT(HMODULE hModule, LPHOOKAPI pHookApi)
{
	// ��λ szImportMod ��������������ݶ��е� IAT ��ַ��
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = LocationIAT(hModule, pHookApi->szDllName);
	if (pImportDesc == NULL) 
	{
		MessageBox(NULL,"fail to find import descriptor","Error",MB_OK);
		return false;
	}
	
	bool result = false;
	if (NULL == pHookApi->pOldProc)
	{
		pHookApi->pOldProc = GetProcAddress(GetModuleHandle(pHookApi->szDllName), pHookApi->szFuncName);
	}

	// ��һ�� Thunk ��ַ��
	PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((DWORD)hModule + (DWORD)(pImportDesc->OriginalFirstThunk));
	// ��һ�� IAT ��� Thunk ��ַ��
	PIMAGE_THUNK_DATA pRealThunk = (PIMAGE_THUNK_DATA)((DWORD)hModule + (DWORD)(pImportDesc->FirstThunk));
	// ѭ�����ұ��� API ������ IAT ���ʹ�����������ַ�޸���ֵ��
	while (pOrigThunk->u1.Function)
	{
		// ���� Thunk �Ƿ�Ϊ IAT �
		if ((pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != IMAGE_ORDINAL_FLAG)
		{
			// ��ȡ�� IAT ���������ĺ������ơ�
			PIMAGE_IMPORT_BY_NAME pByName = (PIMAGE_IMPORT_BY_NAME)((DWORD)hModule + (DWORD)(pOrigThunk->u1.AddressOfData));
			if (pByName->Name[0] == '//0') 
			{
				return false;
			}

			// ����Ƿ�Ϊ���غ�����
			if (_strcmpi(pHookApi->szFuncName, (char*)pByName->Name) == 0)
			{
				MEMORY_BASIC_INFORMATION mbi_thunk;
				// ��ѯ�޸�ҳ����Ϣ��
				VirtualQuery(pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION));
				// �ı��޸�ҳ��������Ϊ PAGE_READWRITE ��
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, PAGE_READWRITE, &mbi_thunk.Protect);

				//�޸�API����IAT������Ϊ���������ַ��
				pRealThunk->u1.Function = (DWORD)pHookApi->pOldProc;

				//�ָ��޸�ҳ�������ԡ�
				DWORD dwOldProtect;
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, mbi_thunk.Protect, &dwOldProtect);
				
				result = true;
				break;
			}
		}
		else
		{
			// ���� Thunk �Ƿ�Ϊ IAT �
			if (pRealThunk->u1.Function == (DWORD)pHookApi->pNewProc)
			{
				MEMORY_BASIC_INFORMATION mbi_thunk;
				// ��ѯ�޸�ҳ����Ϣ��
				VirtualQuery(pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION));
				// �ı��޸�ҳ��������Ϊ PAGE_READWRITE ��
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, PAGE_READWRITE, &mbi_thunk.Protect);

				//�޸�API����IAT������Ϊ���������ַ��
				pRealThunk->u1.Function = (DWORD)pHookApi->pOldProc;

				//�ָ��޸�ҳ�������ԡ�
				DWORD dwOldProtect;
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, mbi_thunk.Protect, &dwOldProtect);

				result = true;
				break;
			}
		}
		
		pOrigThunk++;
		pRealThunk++;
	}

	SetLastError(ERROR_SUCCESS); //���ô���ΪERROR_SUCCESS����ʾ�ɹ���
	return result;
}

static bool HookAPIByIAT(HMODULE hModule, LPHOOKAPI pHookApi)
// ���У� hModule Ϊ����ģ������ szImportMod Ϊ��������ƣ� pHookAPI Ϊ HOOKAPI �ṹָ�롣
{
	// ��λ szImportMod ��������������ݶ��е� IAT ��ַ��
	PIMAGE_IMPORT_DESCRIPTOR pImportDesc = LocationIAT(hModule, pHookApi->szDllName);
	if (pImportDesc == NULL) 
	{
		MessageBox(NULL,"fail to find import descriptor","Error",MB_OK);
		return false;
	}
	
	bool result = false;
	if (NULL == pHookApi->pOldProc)
	{
		pHookApi->pOldProc = GetProcAddress(GetModuleHandle(pHookApi->szDllName), pHookApi->szFuncName);
	}

	// ��һ�� Thunk ��ַ��
	PIMAGE_THUNK_DATA pOrigThunk = (PIMAGE_THUNK_DATA)((DWORD)hModule + (DWORD)(pImportDesc->OriginalFirstThunk));
	// ��һ�� IAT ��� Thunk ��ַ��
	PIMAGE_THUNK_DATA pRealThunk = (PIMAGE_THUNK_DATA)((DWORD)hModule + (DWORD)(pImportDesc->FirstThunk));
	// ѭ�����ұ��� API ������ IAT ���ʹ�����������ַ�޸���ֵ��
	while (pOrigThunk->u1.Function)
	{
		// ���� Thunk �Ƿ�Ϊ IAT �
		if ((pOrigThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) != IMAGE_ORDINAL_FLAG)
		{
			// ��ȡ�� IAT ���������ĺ������ơ�
			PIMAGE_IMPORT_BY_NAME pByName = (PIMAGE_IMPORT_BY_NAME)((DWORD)hModule + (DWORD)(pOrigThunk->u1.AddressOfData));
			if (pByName->Name[0] == '//0') 
			{
				return false;
			}

			// ����Ƿ�Ϊ���غ�����
			if (_strcmpi(pHookApi->szFuncName, (char*)pByName->Name) == 0)
			{
				MEMORY_BASIC_INFORMATION mbi_thunk;
				// ��ѯ�޸�ҳ����Ϣ��
				VirtualQuery(pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION));
				// �ı��޸�ҳ��������Ϊ PAGE_READWRITE ��
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, PAGE_READWRITE, &mbi_thunk.Protect);

				// ����ԭ���� API ������ַ��
				if (pHookApi->pOldProc == NULL)
					pHookApi->pOldProc = (PROC)pRealThunk->u1.Function;

				//�޸�API����IAT������Ϊ���������ַ��
				pRealThunk->u1.Function = (DWORD)pHookApi->pNewProc;

				//�ָ��޸�ҳ�������ԡ�
				DWORD dwOldProtect;
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, mbi_thunk.Protect, &dwOldProtect);
				
				result = true;
				break;
			}
		}
		else
		{
			// ���� Thunk �Ƿ�Ϊ IAT �
			if (pRealThunk->u1.Function == (DWORD)pHookApi->pOldProc)
			{
				MEMORY_BASIC_INFORMATION mbi_thunk;
				// ��ѯ�޸�ҳ����Ϣ��
				VirtualQuery(pRealThunk, &mbi_thunk, sizeof(MEMORY_BASIC_INFORMATION));
				// �ı��޸�ҳ��������Ϊ PAGE_READWRITE ��
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, PAGE_READWRITE, &mbi_thunk.Protect);

				//�޸�API����IAT������Ϊ���������ַ��
				pRealThunk->u1.Function = (DWORD)pHookApi->pNewProc;

				//�ָ��޸�ҳ�������ԡ�
				DWORD dwOldProtect;
				VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, mbi_thunk.Protect, &dwOldProtect);
					
				pHookApi->result = true;
				result = true;
				break;
			}
		}
		
		pOrigThunk++;
		pRealThunk++;
	}

	SetLastError(ERROR_SUCCESS); //���ô���ΪERROR_SUCCESS����ʾ�ɹ���
	pHookApi->result = result;
	return result;
}

static bool UnHookAPIByJmp(HMODULE hModule, LPHOOKAPI pHookApi)
{
    DWORD dwOldProtect = 0;
    ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	/*�ָ�ԭʼ����*/
    if (!WriteProcessMemory(g_process_hdl, (void*)pHookApi->pOldProc,
					(void*)pHookApi->oldFirstCode, pHookApi->codeLen, NULL))
    {
        ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
        return false;
    }

    ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
    return true;
}

static bool HookAPIByJmp(HMODULE hModule, LPHOOKAPI pHookApi)
{
	#if 1
	pHookApi->newFirstCode[0] = 0xe9;//ʵ����0xe9���൱��jmpָ��   
	pHookApi->newFirstCode[1] = 0x00;
	pHookApi->newFirstCode[2] = 0x00;
	pHookApi->newFirstCode[3] = 0x00;
	pHookApi->newFirstCode[4] = 0x00;
	pHookApi->newFirstCode[5] = 0x00;
	pHookApi->newFirstCode[6] = 0x00;
	pHookApi->newFirstCode[7] = 0x00;
	pHookApi->codeLen = 5;
	#else
	pHookApi->newFirstCode[0] = 0x68;//push xxxx; ret
	pHookApi->newFirstCode[1] = 0x00;
	pHookApi->newFirstCode[2] = 0x00;
	pHookApi->newFirstCode[3] = 0x00;
	pHookApi->newFirstCode[4] = 0x00;
	pHookApi->newFirstCode[5] = 0xc3;
	pHookApi->newFirstCode[6] = 0x00;
	pHookApi->newFirstCode[7] = 0x00;
	#endif

	if (NULL == pHookApi->pOldProc)
	{
		pHookApi->pOldProc = GetProcAddress(GetModuleHandle(pHookApi->szDllName), pHookApi->szFuncName);
	}

	DWORD dwOldProtect = 0;
	::VirtualProtect((LPVOID)pHookApi->pOldProc, pHookApi->codeLen, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	// ���ȱ��汻�ƻ���ָ��
	// memcpy(pHookApi->oldFirstCode, pHookApi->pOldProc, 5); 
	BOOL bOk = false;
	bOk = ReadProcessMemory(g_process_hdl, (void*)pHookApi->pOldProc,
					(void*)pHookApi->oldFirstCode, pHookApi->codeLen, NULL);
	if (!bOk)
	{
		::VirtualProtect((LPVOID)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
		MessageBox(NULL,"ReadProcessMemory failed","Error",MB_OK);
		return false;
	}

	//��HOOK�ĺ���ͷ��Ϊjmp
	*(DWORD*)(&pHookApi->newFirstCode[1]) = (DWORD)pHookApi->pNewProc - ((DWORD)pHookApi->pOldProc + 5);

	//д���µ�ַ
	bOk = WriteProcessMemory(g_process_hdl, (void*)pHookApi->pOldProc,
					(void*)pHookApi->newFirstCode, pHookApi->codeLen, NULL);
	if (!bOk)
	{
		::VirtualProtect((LPVOID)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
		MessageBox(NULL,"WriteProcessMemory failed","Error",MB_OK);
		return false;
	}

	::VirtualProtect((LPVOID)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);

	pHookApi->result = true;
	return true;
}

static bool HookAPIResumeByJmp(LPHOOKAPI pHookApi)
{
	if (pHookApi->codeLen == 0)
	{
		return true;
	}

    DWORD dwOldProtect = 0;
    ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	/*�ָ�ԭʼ����*/
    if (!WriteProcessMemory(g_process_hdl, (void*)pHookApi->pOldProc,
					(void*)pHookApi->oldFirstCode, pHookApi->codeLen, NULL))
    {
        ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
        return false;
    }

    ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
    return true;
}

static bool HookAPIRecoveryByJmp(LPHOOKAPI pHookApi)
{
	if (pHookApi->codeLen == 0)
	{
		return true;
	}

    DWORD dwOldProtect = 0;
    ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, PAGE_EXECUTE_READWRITE, &dwOldProtect);

	/*�ָ�ԭʼ����*/
    if (!WriteProcessMemory(g_process_hdl, (void*)pHookApi->pOldProc,
					(void*)pHookApi->newFirstCode, pHookApi->codeLen, NULL))
    {
        ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
        return false;
    }

    ::VirtualProtect((void*)pHookApi->pOldProc, pHookApi->codeLen, dwOldProtect, NULL);
    return true;
}


static bool HookAPIByMhook(HMODULE hModule, LPHOOKAPI pHookApi)
{
	if (NULL == pHookApi->pOldProc)
	{
		pHookApi->pOldProc = GetProcAddress(GetModuleHandle(pHookApi->szDllName), pHookApi->szFuncName);
	}

	if (TRUE == Mhook_SetHook((PVOID*)&pHookApi->pOldProc, pHookApi->pNewProc))
	{
		pHookApi->result = true;
		return true;
	}

	return false;
}

static bool UnHookAPIByMhook(HMODULE hModule, LPHOOKAPI pHookApi)
{
	return TRUE == Mhook_Unhook((PVOID*)&pHookApi->pOldProc);
}


bool HookAPI(HMODULE hModule, LPHOOKAPI pHookApi)
{
	if (pHookApi->type == HOOK_MHOOK)
	{
		return HookAPIByMhook(hModule, pHookApi);
	}
	else if(pHookApi->type == HOOK_JMP)
	{
		return HookAPIByJmp(hModule, pHookApi);
	}
	return HookAPIByIAT(hModule, pHookApi);
}


bool UnHookAPI(HMODULE hModule, LPHOOKAPI pHookApi)
{
	if (pHookApi->type == HOOK_MHOOK)
	{
		return UnHookAPIByMhook(hModule, pHookApi);
	}
	else if (pHookApi->type == HOOK_JMP)
	{
		return UnHookAPIByJmp(hModule, pHookApi);
	}
	return UnHookAPIByIAT(hModule, pHookApi);
}

bool HookAPIRecovery(LPHOOKAPI pHookApi)
{
	if (pHookApi->type == HOOK_JMP)
	{
		return HookAPIRecoveryByJmp(pHookApi);
	}

	return true;
}

bool HookAPIResume(LPHOOKAPI pHookApi)
{
	if (pHookApi->type == HOOK_JMP)
	{
		return HookAPIResumeByJmp(pHookApi);
	}

	return true;
}
