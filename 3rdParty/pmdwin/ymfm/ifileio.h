//=============================================================================
//		File I/O Interface Class : IFILEIO
//=============================================================================

#ifndef IFILEIO_H
#define IFILEIO_H

#include "portability_opna.h"
#include "comsupport.h"

//=============================================================================
//	COM 風 interface class(File I/O interface)
//=============================================================================
interface IFILEIO : public IUnknown {
public:
	enum Flags
	{
		flags_open		= 0x000001,
		flags_readonly	= 0x000002,
		flags_create	= 0x000004,
	};
	
	enum SeekMethod
	{
		seekmethod_begin = 0, seekmethod_current = 1, seekmethod_end = 2,
	};
	
	enum Error
	{
		error_success = 0,
		error_file_not_found,
		error_sharing_violation,
		error_unknown = -1
	};
	
	virtual int64_t WINAPI GetFileSize(const TCHAR* filename) = 0;
	virtual bool WINAPI Open(const TCHAR* filename, uint flg = 0) = 0;
	virtual void WINAPI Close() = 0;
	virtual int32_t WINAPI Read(void* dest, int32_t len) = 0;
	virtual bool WINAPI Seek(int32_t fpos, SeekMethod method) = 0;
	virtual int32_t WINAPI Tellp() = 0;
};


//=============================================================================
//	COM 風 interface class(IFILEIO 設定)
//=============================================================================
interface ISETFILEIO : public IUnknown {
public:
	virtual void WINAPI setfileio(IFILEIO* pfileio) = 0;
};


//=============================================================================
//	Interface ID(IID) & Class ID(CLSID)
//=============================================================================

#ifdef _WIN32
	
	// GUID of IFILEIO Interface ID
	interface	__declspec(uuid("A484553E-1CC4-4365-A65D-57287F2007EA")) IFILEIO;
	
	// GUID of FILEIO Class ID
	class		__declspec(uuid("6C242609-3D31-4124-B871-2403C396B776")) FILEIO;
	
	
	const IID	IID_IFILEIO		= _uuidof(IFILEIO);		// IFILEIO Interface ID
	const CLSID	CLSID_FILEIO	= _uuidof(FILEIO);		// FILEIO Class ID
	
	
	// GUID of IFILESTREAM Interface ID
	interface	__declspec(uuid("C51400C3-F1B7-459F-8883-74226981855E")) ISETFILEIO;
	
	// GUID of FILESTREAM Class ID
	class		__declspec(uuid("2B73EEE1-4465-4DA1-85A7-6DDB1E3C244B")) FILESTREAM;
	
	
	const IID	IID_IFILESTREAM = _uuidof(ISETFILEIO);	// IFILESTREAM Interface ID
	const CLSID	CLSID_FILESTREAM = _uuidof(FILESTREAM);	// FILESTREAM Class ID
	

#else
	// GUID of IFILEIO Interface ID
	const IID	IID_IFILEIO			= {0xA484553E, 0x1CC4, 0x4365, {0xA6, 0x5D, 0x57, 0x28, 0x7F, 0x20, 0x07, 0xEA}};
	
	// GUID of FILEIO Class ID
	const CLSID	CLSID_FILEIO		= {0x6C242609, 0x3D31, 0x4124, {0xB8, 0x71, 0x24, 0x03, 0xC3, 0x96, 0xB7, 0x76}};
	
	
	// GUID of IFILESTREAM Interface ID
	const IID	IID_IFILESTREAM		= {0xC51400C3, 0xF1B7, 0x459F, {0x88, 0x83, 0x74, 0x22, 0x69, 0x81, 0x85, 0x5E}};
	
	// GUID of FILESTREAM Class ID
	const CLSID	CLSID_FILESTREAM	= {0x2B73EEE1, 0x4465, 0x4DA1, {0x85, 0xA7, 0x6D, 0xDB, 0x1E, 0x3C, 0x24, 0x4B}};
	


#endif


#endif	// IFILEIO_H
