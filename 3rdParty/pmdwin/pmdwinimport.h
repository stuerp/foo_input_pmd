//=============================================================================
//	Professional Music Driver [P.M.D.] version 4.8
//			DLL Import Header
//			Programmed By M.Kajihara
//			Windows Converted by C60
//=============================================================================

#ifndef PMDWINIMPORT_H
#define PMDWINIMPORT_H

#include "ipmdwin.h"


//=============================================================================
//	DLL Import Functions
//=============================================================================

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllimport) int WINAPI getversion(void);
__declspec(dllimport) int WINAPI getinterfaceversion(void);
__declspec(dllimport) bool WINAPI pmdwininit(TCHAR *path);
__declspec(dllimport) bool WINAPI loadrhythmsample(TCHAR *path);
__declspec(dllimport) bool WINAPI setpcmdir(TCHAR **path);
__declspec(dllimport) void WINAPI setpcmrate(int rate);
__declspec(dllimport) void WINAPI setppzrate(int rate);
__declspec(dllimport) void WINAPI setppsuse(bool value);
__declspec(dllimport) void WINAPI setrhythmwithssgeffect(bool value);
__declspec(dllimport) void WINAPI setpmd86pcmmode(bool value);
__declspec(dllimport) bool WINAPI getpmd86pcmmode(void);
__declspec(dllimport) int WINAPI music_load(TCHAR *filename);
__declspec(dllimport) int WINAPI music_load2(uint8_t *musdata, int size);
__declspec(dllimport) void WINAPI music_start(void);
__declspec(dllimport) void WINAPI music_stop(void);
__declspec(dllimport) void WINAPI fadeout(int speed);
__declspec(dllimport) void WINAPI fadeout2(int speed);
__declspec(dllimport) void WINAPI getpcmdata(int16_t *buf, int nsamples);
__declspec(dllimport) void WINAPI setfmcalc55k(bool flag);
__declspec(dllimport) void WINAPI setppsinterpolation(bool ip);
__declspec(dllimport) void WINAPI setp86interpolation(bool ip);
__declspec(dllimport) void WINAPI setppzinterpolation(bool ip);
__declspec(dllimport) char * WINAPI getmemo(char *dest, uint8_t *musdata, int size, int al);
__declspec(dllimport) char * WINAPI getmemo2(char *dest, uint8_t *musdata, int size, int al);
__declspec(dllimport) char * WINAPI getmemo3(char *dest, uint8_t *musdata, int size, int al);
__declspec(dllimport) int WINAPI fgetmemo(char *dest, TCHAR *filename, int al);
__declspec(dllimport) int WINAPI fgetmemo2(char *dest, TCHAR *filename, int al);
__declspec(dllimport) int WINAPI fgetmemo3(char *dest, TCHAR *filename, int al);
__declspec(dllimport) TCHAR * WINAPI getmusicfilename(TCHAR *dest);
__declspec(dllimport) TCHAR * WINAPI getpcmfilename(TCHAR *dest);
__declspec(dllimport) TCHAR * WINAPI getppcfilename(TCHAR *dest);
__declspec(dllimport) TCHAR * WINAPI getppsfilename(TCHAR *dest);
__declspec(dllimport) TCHAR * WINAPI getp86filename(TCHAR *dest);
__declspec(dllimport) TCHAR * WINAPI getppzfilename(TCHAR *dest, int bufnum);
__declspec(dllimport) int WINAPI ppc_load(TCHAR *filename);
__declspec(dllimport) int WINAPI pps_load(TCHAR *filename);
__declspec(dllimport) int WINAPI p86_load(TCHAR *filename);
__declspec(dllimport) int WINAPI ppz_load(TCHAR *filename, int bufnum);
__declspec(dllimport) int WINAPI maskon(int ch);
__declspec(dllimport) int WINAPI maskoff(int ch);
__declspec(dllimport) void WINAPI setfmvoldown(int voldown);
__declspec(dllimport) void WINAPI setssgvoldown(int voldown);
__declspec(dllimport) void WINAPI setrhythmvoldown(int voldown);
__declspec(dllimport) void WINAPI setadpcmvoldown(int voldown);
__declspec(dllimport) void WINAPI setppzvoldown(int voldown);
__declspec(dllimport) int WINAPI getfmvoldown(void);
__declspec(dllimport) int WINAPI getfmvoldown2(void);
__declspec(dllimport) int WINAPI getssgvoldown(void);
__declspec(dllimport) int WINAPI getssgvoldown2(void);
__declspec(dllimport) int WINAPI getrhythmvoldown(void);
__declspec(dllimport) int WINAPI getrhythmvoldown2(void);
__declspec(dllimport) int WINAPI getadpcmvoldown(void);
__declspec(dllimport) int WINAPI getadpcmvoldown2(void);
__declspec(dllimport) int WINAPI getppzvoldown(void);
__declspec(dllimport) int WINAPI getppzvoldown2(void);
__declspec(dllimport) void WINAPI setpos(int pos);
__declspec(dllimport) void WINAPI setpos2(int pos);
__declspec(dllimport) int WINAPI getpos(void);
__declspec(dllimport) int WINAPI getpos2(void);
__declspec(dllimport) bool WINAPI getlength(TCHAR *filename, int *length, int *loop);
__declspec(dllimport) bool WINAPI getlength2(TCHAR *filename, int *length, int *loop);
__declspec(dllimport) int WINAPI getloopcount(void);
__declspec(dllimport) void WINAPI setfmwait(int nsec);
__declspec(dllimport) void WINAPI setssgwait(int nsec);
__declspec(dllimport) void WINAPI setrhythmwait(int nsec);
__declspec(dllimport) void WINAPI setadpcmwait(int nsec);
__declspec(dllimport) OPEN_WORK * WINAPI getopenwork(void);
__declspec(dllimport) QQ * WINAPI getpartwork(int ch);

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


#endif // PMDWINIMPORT_H
