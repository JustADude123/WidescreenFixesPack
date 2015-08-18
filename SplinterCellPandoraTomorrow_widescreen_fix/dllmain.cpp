#include "stdafx.h"
#include "stdio.h"
#include "..\includes\injector\injector.hpp"
#include "..\includes\IniReader.h"

HWND hWnd;
void ProgramRestart(LPTSTR lpszTitle);

#define _USE_MATH_DEFINES
#include "math.h"
#define DEGREE_TO_RADIAN(fAngle) \
	((fAngle)* (float)M_PI / 180.0f)
#define RADIAN_TO_DEGREE(fAngle) \
	((fAngle)* 180.0f / (float)M_PI)
#define SCREEN_AR_NARROW			(4.0f / 3.0f)	// 640.0f / 480.0f
#define SCREEN_FOV_HORIZONTAL		75.0f
#define SCREEN_FOV_VERTICAL			(2.0f * RADIAN_TO_DEGREE(atan(tan(DEGREE_TO_RADIAN(SCREEN_FOV_HORIZONTAL * 0.5f)) / SCREEN_AR_NARROW)))	// Default is 75.0f.
float fScreenFieldOfViewVStd = SCREEN_FOV_VERTICAL;
float fDynamicScreenFieldOfViewScale;

struct Screen
{
	int Width;
	int Height;
	float fWidth;
	float fHeight;
	float fFieldOfView;
	float fAspectRatio;
	float fHudOffset;
	float fHudOffsetRight;
	float fFMVoffsetStartX;
	float fFMVoffsetEndX;
	float fFMVoffsetStartY;
	float fFMVoffsetEndY;
} Screen;

HMODULE D3DDrv, WinDrv, Engine;
DWORD hookJmpAddr, hookJmpAddr2, hookJmpAddr3, hookJmpAddr4, hookJmpAddr5, hookJmpAddr6;
DWORD dword_10173E5C;
DWORD nForceShadowBufferSupport, nFMVWidescreenMode;

void __declspec(naked) UD3DRenderDevice_SetRes_Hook()
{
	_asm
	{
		mov     edx, dword ptr ds:[dword_10173E5C]
		mov		eax, Screen.Height
		mov     dword ptr ds : [edx], eax // 640
		mov     edx, Screen.Width
		jmp	    hookJmpAddr
	}
}

void __declspec(naked) UWindowsViewport_ResizeViewport_Hook()
{
	_asm
	{
		mov     ecx, Screen.Height
		mov     [esi + 84h], ecx
		mov		edx, Screen.Width
		jmp	    hookJmpAddr2
	}
}

DWORD __esp;
float offset1, offset2;
DWORD Color;
void CenterHud()
{
	offset1 = *(float*)(__esp + 4);
	offset2 = *(float*)(__esp + 4 + 8);
	Color = *(DWORD*)(__esp + 0x2C);

	/*if (offset1 >= 780.0f && offset1 <= 925.0f) //898.50 912.00 916.50 910.50 906.0 904.5 780.00 792.00 787.50 915.0//925 - everything cept interaction menu
	{
		offset2 += 150.0f;
		offset1 += 150.0f;
	}*/

	offset1 += Screen.fHudOffset;
	offset2 += Screen.fHudOffset;

	if (Color == 0xFE000000)
	{
		offset2 *= 0.0f; // hiding cutscene borders
		offset1 *= 0.0f;
	}
}

void __declspec(naked) FCanvasUtil_DrawTile_Hook()
{
	_asm
	{
		mov  __esp, esp
		call CenterHud
		mov  eax, offset1
		mov  [esp+4], eax;
		mov  eax, offset2
		mov  [esp + 4 + 8], eax;
		jmp	 hookJmpAddr3
	}
}

float __ECX;
void __declspec(naked) UGameEngine_Draw_Hook()
{
	_asm
	{
		mov  ecx, [eax + 374h]
		mov  __ECX, ecx
	}
	__ECX *= fDynamicScreenFieldOfViewScale;
	__asm   mov ecx, __ECX
	__asm	jmp	 hookJmpAddr4
}


void __declspec(naked) OpenVideo_Hook()
{
	__asm   mov     ecx, 0x4B000 //640x480
	__asm	shr     ecx, 2
	__asm	xor     eax, eax
	__asm	jmp	 hookJmpAddr5
}

float f1 = 1.0f;
void __declspec(naked) DisplayVideo_Hook()
{
	_asm
	{
		fstp    dword ptr[esp]
		push    esi
		push    esi
		mov		__ECX, ecx
		mov  ecx, f1
		mov[esp + 20h], ecx;
		mov[esp + 24h], ecx;
		mov ecx, nFMVWidescreenMode
		cmp ecx, 0
	jz label1
		mov ecx, Screen.fFMVoffsetStartY
		mov[esp + 4h], ecx;
		mov ecx, Screen.fFMVoffsetEndY
		mov[esp + 0xC], ecx;
	jmp label2
	label1:
		mov ecx, Screen.fFMVoffsetStartX
		mov[esp + 0h], ecx;
		mov ecx, Screen.fFMVoffsetEndX
		mov[esp + 8h], ecx;
	label2:
		mov	 ecx, __ECX
		jmp	 hookJmpAddr6
	}
}
	
DWORD WINAPI Thread(LPVOID)
{
	CIniReader iniReader("");
	Screen.Width = iniReader.ReadInteger("MAIN", "ResX", 0);
	Screen.Height = iniReader.ReadInteger("MAIN", "ResY", 0);
	nForceShadowBufferSupport = iniReader.ReadInteger("MAIN", "ForceShadowBufferSupport", 0);
	nFMVWidescreenMode = iniReader.ReadInteger("MAIN", "FMVWidescreenMode", 0);

	if (!Screen.Width || !Screen.Height) {
		HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO info;
		info.cbSize = sizeof(MONITORINFO);
		GetMonitorInfo(monitor, &info);
		Screen.Width = info.rcMonitor.right - info.rcMonitor.left;
		Screen.Height = info.rcMonitor.bottom - info.rcMonitor.top;
	}

	Screen.fWidth = static_cast<float>(Screen.Width);
	Screen.fHeight = static_cast<float>(Screen.Height);
	Screen.fAspectRatio = (Screen.fWidth / Screen.fHeight);

	while (true)
	{
		Sleep(0);
		D3DDrv = GetModuleHandle("D3DDrv");
		WinDrv = GetModuleHandle("WinDrv");
		Engine = GetModuleHandle("Engine");
		if (D3DDrv && WinDrv && Engine)
			break;
	}

	DWORD pfSetRes = injector::ReadMemory<DWORD>((DWORD)GetProcAddress(D3DDrv, "?SetRes@UD3DRenderDevice@@UAEHPAVUViewport@@HHH@Z") + 0x1, true) + (DWORD)GetProcAddress(D3DDrv, "?SetRes@UD3DRenderDevice@@UAEHPAVUViewport@@HHH@Z") + 5;
	dword_10173E5C = injector::ReadMemory<DWORD>(pfSetRes + 0x435 + 0x1, true);
	injector::MakeJMP(pfSetRes + 0x435, UD3DRenderDevice_SetRes_Hook, true);
	hookJmpAddr = pfSetRes + 0x435 + 0x5;
	
	/*DWORD pfResizeViewport = (DWORD)GetProcAddress(WinDrv, "?ResizeViewport@UWindowsViewport@@UAEHKHH@Z");
	injector::MakeJMP(pfResizeViewport + 0x469, UWindowsViewport_ResizeViewport_Hook, true); //crash on FMV
	hookJmpAddr2 = pfResizeViewport + 0x469 + 0x6;*/

	DWORD pfDrawTile = (DWORD)GetProcAddress(Engine, "?DrawTile@FCanvasUtil@@QAEXMMMMMMMMMPAVUMaterial@@VFColor@@HH@Z");
	DWORD pfFUCanvasDrawTile = (DWORD)GetProcAddress(Engine, "?DrawTile@UCanvas@@UAEXPAVUMaterial@@MMMMMMMMMVFPlane@@1H@Z");
	DWORD pfexecDrawTextClipped = (DWORD)GetProcAddress(Engine, "?execDrawTextClipped@UCanvas@@QAEXAAUFFrame@@QAX@Z");
	DWORD pfsub_103762F0 = injector::ReadMemory<DWORD>(pfexecDrawTextClipped + 0xF4 + 0x1, true) + pfexecDrawTextClipped + 0xF4 + 5;
	static double HUDScaleX = 1.0f / Screen.fWidth * (Screen.fHeight / 480.0f);
	injector::WriteMemory<double>(injector::ReadMemory<DWORD>(pfDrawTile + 0x49 + 0x2, true), HUDScaleX, true);

	injector::MakeCALL(pfFUCanvasDrawTile + 0x219, FCanvasUtil_DrawTile_Hook, true);
	injector::MakeCALL(pfsub_103762F0 + 0x36E, FCanvasUtil_DrawTile_Hook, true);
	injector::MakeCALL(pfsub_103762F0 + 0x43D, FCanvasUtil_DrawTile_Hook, true);
	injector::MakeCALL(pfsub_103762F0 + 0x4DA, FCanvasUtil_DrawTile_Hook, true);
	injector::MakeCALL(pfsub_103762F0 + 0x564, FCanvasUtil_DrawTile_Hook, true);
	hookJmpAddr3 = (DWORD)GetProcAddress(Engine, "?DrawTile@FCanvasUtil@@QAEXMMMMMMMMMPAVUMaterial@@VFColor@@HH@Z");

	//FMV
	DWORD pfOpenVideo = injector::ReadMemory<DWORD>((DWORD)GetProcAddress(D3DDrv, "?OpenVideo@UD3DRenderDevice@@UAEXPAVUCanvas@@PADHHH@Z") + 0x1, true) + (DWORD)GetProcAddress(D3DDrv, "?OpenVideo@UD3DRenderDevice@@UAEXPAVUCanvas@@PADHHH@Z") + 5;
	DWORD pfDisplayVideo = injector::ReadMemory<DWORD>((DWORD)GetProcAddress(D3DDrv, "?DisplayVideo@UD3DRenderDevice@@UAEXPAVUCanvas@@PAX@Z") + 0x1, true) + (DWORD)GetProcAddress(D3DDrv, "?DisplayVideo@UD3DRenderDevice@@UAEXPAVUCanvas@@PAX@Z") + 5;
	//////injector::WriteMemory<float>(injector::ReadMemory<DWORD>((DWORD)GetProcAddress(D3DDrv, "?DisplayVideo@UD3DRenderDevice@@UAEXPAVUCanvas@@PAX@Z") + 0x55D, true), 0.0f, true); //Y 
	//injector::WriteMemory<float>(injector::ReadMemory<DWORD>(pfDisplayVideo + 0x332 + 0x2, true), 0.003125f, true); //X
	injector::MakeJMP(pfOpenVideo + 0x2D4, OpenVideo_Hook, true);
	hookJmpAddr5 = pfOpenVideo + 0x2D4 + 0x5;
	injector::MakeJMP(pfDisplayVideo + 0x37E, DisplayVideo_Hook, true);
	hookJmpAddr6 = pfDisplayVideo + 0x37E + 0x5;
	Screen.fFMVoffsetStartX = (Screen.fWidth - Screen.fHeight * (4.0f / 3.0f)) / 2.0f;
	Screen.fFMVoffsetEndX = Screen.fWidth - Screen.fFMVoffsetStartX;
	Screen.fFMVoffsetStartY = 0.0f - ((Screen.fHeight - ((Screen.fHeight / 1.5f) * ((16.0f / 9.0f) / Screen.fAspectRatio))) / 2.0f);
	Screen.fFMVoffsetEndY = Screen.fHeight - Screen.fFMVoffsetStartY;

	//HUD
	Screen.fHudOffset = (Screen.fWidth - Screen.fHeight * (4.0f/3.0f)) / 2.0f / (Screen.fWidth / (640.0f * (4.0f / 3.0f)));

	//FOV
	DWORD pfDraw = (DWORD)GetProcAddress(Engine, "?Draw@UGameEngine@@UAEXPAVUViewport@@HPAEPAH@Z");
	fDynamicScreenFieldOfViewScale = 2.0f * RADIAN_TO_DEGREE(atan(tan(DEGREE_TO_RADIAN(fScreenFieldOfViewVStd * 0.5f)) * Screen.fAspectRatio)) * (1.0f / SCREEN_FOV_HORIZONTAL);
	injector::MakeJMP(pfDraw + 0x167, UGameEngine_Draw_Hook, true);
	hookJmpAddr4 = pfDraw + 0x167 + 0x6;

	//Shadows
	if (nForceShadowBufferSupport)
	{
		DWORD pfSupportsShadowBuffer = injector::ReadMemory<DWORD>((DWORD)GetProcAddress(D3DDrv, "?SupportsShadowBuffer@UD3DRenderDevice@@QAEHXZ") + 0x1, true) + (DWORD)GetProcAddress(D3DDrv, "?SupportsShadowBuffer@UD3DRenderDevice@@QAEHXZ") + 5;
		injector::WriteMemory<unsigned short>(pfSupportsShadowBuffer + 0x10, 0xE990, true);
		injector::WriteMemory(pfSupportsShadowBuffer + 0x113, &nForceShadowBufferSupport, true);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE /*hModule*/, DWORD reason, LPVOID /*lpReserved*/)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DWORD fAttr = GetFileAttributes(".\\SplinterCell2.ini");
		char* UserIni;
		if ((fAttr != INVALID_FILE_ATTRIBUTES) && !(fAttr & FILE_ATTRIBUTE_DIRECTORY))
		{
			UserIni = ".\\SplinterCell2.ini";
		}
		else
		{
			UserIni = "..\\SplinterCell2.ini";
		}
		CIniReader iniWriter(UserIni);
		char* Res = iniWriter.ReadString("WinDrv.WindowsClient", "WindowedViewportX", "");

		if (strcmp(Res, "1920") != 0)
		{
			iniWriter.WriteString("WinDrv.WindowsClient", "WindowedViewportX", "1920");
			iniWriter.WriteString("WinDrv.WindowsClient", "WindowedViewportY", "1080");
			ProgramRestart("SCPT Widescreen Fix");
		}

		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&Thread, NULL, 0, NULL);
	}
	return TRUE;
}

void ProgramRestart(LPTSTR lpszTitle)
{
	// Just doing this so you can tell that is a new process
	TCHAR szBuffer[512];
	if (strlen(lpszTitle) > 500) lpszTitle[500] = 0;
	wsprintf(szBuffer, TEXT("%s - %08X"), lpszTitle, GetCurrentProcessId());

	// If they answer yes, launch the new process
	if (MessageBox(HWND_DESKTOP, TEXT("Widescreen fix detected that custom resolution is set in SplinterCell2.ini. It has been removed to make the fix work properly, the game should be restarted for the changes to take effect. Restart now?"),
		szBuffer, MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND | MB_APPLMODAL) == IDYES)
	{
		TCHAR szPath[MAX_PATH + 1];
		PROCESS_INFORMATION pi;
		STARTUPINFO si;

		GetStartupInfo(&si);
		GetModuleFileName(NULL, szPath, MAX_PATH);
		static LPTSTR lpszRestartMutex = TEXT("NapalmSelfRestart");
		HANDLE hRestartMutex = CreateMutex(NULL, TRUE, lpszRestartMutex);

		if (CreateProcess(szPath, GetCommandLine(), NULL, NULL,
			FALSE, DETACHED_PROCESS, NULL, NULL, &si, &pi) == 0)
		{
			MessageBox(HWND_DESKTOP, TEXT("Failed to restart program.\n"
				"Please try manually."), TEXT("Error"), MB_ICONERROR);
		}
		else {
			Sleep(1000);
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
		}
	}
}