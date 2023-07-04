//=============================================================================
//	Professional Music Driver [P.M.D.] version 4.8
//			Programmed By M.Kajihara
//			Windows Converted by C60
//=============================================================================

#ifndef PMDWIN_H
#define PMDWIN_H

#include "ipmdwin.h"


//=============================================================================
//	バージョン情報
//=============================================================================
#define	DLLVersion			 51		// 上１桁：major, 下２桁：minor version



#ifdef _WIN32

//=============================================================================
//	DLL Export Functions
//=============================================================================

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) int WINAPI getversion(void);
__declspec(dllexport) int WINAPI getinterfaceversion(void);
__declspec(dllexport) bool WINAPI pmdwininit(TCHAR *path);
__declspec(dllexport) bool WINAPI loadrhythmsample(TCHAR *path);
__declspec(dllexport) bool WINAPI setpcmdir(TCHAR **path);
__declspec(dllexport) void WINAPI setpcmrate(int rate);
__declspec(dllexport) void WINAPI setppzrate(int rate);
__declspec(dllexport) void WINAPI setppsuse(bool value);
__declspec(dllexport) void WINAPI setrhythmwithssgeffect(bool value);
__declspec(dllexport) void WINAPI setpmd86pcmmode(bool value);
__declspec(dllexport) bool WINAPI getpmd86pcmmode(void);
__declspec(dllexport) int WINAPI music_load(TCHAR *filename);
__declspec(dllexport) int WINAPI music_load2(uint8_t *musdata, int size);
__declspec(dllexport) void WINAPI music_start(void);
__declspec(dllexport) void WINAPI music_stop(void);
__declspec(dllexport) void WINAPI fadeout(int speed);
__declspec(dllexport) void WINAPI fadeout2(int speed);
__declspec(dllexport) void WINAPI getpcmdata(int16_t *buf, int nsamples);
__declspec(dllexport) void WINAPI setfmcalc55k(bool flag);
__declspec(dllexport) void WINAPI setppsinterpolation(bool ip);
__declspec(dllexport) void WINAPI setp86interpolation(bool ip);
__declspec(dllexport) void WINAPI setppzinterpolation(bool ip);
__declspec(dllexport) char * WINAPI getmemo(char *dest, uint8_t *musdata, int size, int al);
__declspec(dllexport) char * WINAPI getmemo2(char *dest, uint8_t *musdata, int size, int al);
__declspec(dllexport) char * WINAPI getmemo3(char *dest, uint8_t *musdata, int size, int al);
__declspec(dllexport) int WINAPI fgetmemo(char *dest, TCHAR *filename, int al);
__declspec(dllexport) int WINAPI fgetmemo2(char *dest, TCHAR *filename, int al);
__declspec(dllexport) int WINAPI fgetmemo3(char *dest, TCHAR *filename, int al);
__declspec(dllexport) TCHAR * WINAPI getmusicfilename(TCHAR *dest);
__declspec(dllexport) TCHAR * WINAPI getpcmfilename(TCHAR *dest);
__declspec(dllexport) TCHAR * WINAPI getppcfilename(TCHAR *dest);
__declspec(dllexport) TCHAR * WINAPI getppsfilename(TCHAR *dest);
__declspec(dllexport) TCHAR * WINAPI getp86filename(TCHAR *dest);
__declspec(dllexport) TCHAR * WINAPI getppzfilename(TCHAR *dest, int bufnum);
__declspec(dllexport) int WINAPI ppc_load(TCHAR *filename);
__declspec(dllexport) int WINAPI pps_load(TCHAR *filename);
__declspec(dllexport) int WINAPI p86_load(TCHAR *filename);
__declspec(dllexport) int WINAPI ppz_load(TCHAR *filename, int bufnum);
__declspec(dllexport) int WINAPI maskon(int ch);
__declspec(dllexport) int WINAPI maskoff(int ch);
__declspec(dllexport) void WINAPI setfmvoldown(int voldown);
__declspec(dllexport) void WINAPI setssgvoldown(int voldown);
__declspec(dllexport) void WINAPI setrhythmvoldown(int voldown);
__declspec(dllexport) void WINAPI setadpcmvoldown(int voldown);
__declspec(dllexport) void WINAPI setppzvoldown(int voldown);
__declspec(dllexport) int WINAPI getfmvoldown(void);
__declspec(dllexport) int WINAPI getfmvoldown2(void);
__declspec(dllexport) int WINAPI getssgvoldown(void);
__declspec(dllexport) int WINAPI getssgvoldown2(void);
__declspec(dllexport) int WINAPI getrhythmvoldown(void);
__declspec(dllexport) int WINAPI getrhythmvoldown2(void);
__declspec(dllexport) int WINAPI getadpcmvoldown(void);
__declspec(dllexport) int WINAPI getadpcmvoldown2(void);
__declspec(dllexport) int WINAPI getppzvoldown(void);
__declspec(dllexport) int WINAPI getppzvoldown2(void);
__declspec(dllexport) void WINAPI setpos(int pos);
__declspec(dllexport) void WINAPI setpos2(int pos);
__declspec(dllexport) int WINAPI getpos(void);
__declspec(dllexport) int WINAPI getpos2(void);
__declspec(dllexport) bool WINAPI getlength(TCHAR *filename, int *length, int *loop);
__declspec(dllexport) bool WINAPI getlength2(TCHAR *filename, int *length, int *loop);
__declspec(dllexport) int WINAPI getloopcount(void);
__declspec(dllexport) void WINAPI setfmwait(int nsec);
__declspec(dllexport) void WINAPI setssgwait(int nsec);
__declspec(dllexport) void WINAPI setrhythmwait(int nsec);
__declspec(dllexport) void WINAPI setadpcmwait(int nsec);
__declspec(dllexport) OPEN_WORK * WINAPI getopenwork(void);
__declspec(dllexport) QQ * WINAPI getpartwork(int ch);

__declspec(dllexport) HRESULT WINAPI pmd_CoCreateInstance(
	REFCLSID rclsid,		//Class identifier (CLSID) of the object
	LPUNKNOWN pUnkOuter,	//Pointer to whether object is or isn't part 
							// of an aggregate
	DWORD dwClsContext,		//Context for running executable code
	REFIID riid,			//Reference to the identifier of the interface
	LPVOID * ppv			//Address of output variable that receives 
							// the interface pointer requested in riid
);


#ifdef __cplusplus
}
#endif

#endif

#endif // PMDWIN_H
