#pragma once

#include "AESencSW.h"

#define	AESBLK_SZ		16
#define	AES_BLKS(sz)	((sz + AESBLK_SZ - 1)>>4)		//Round up to closest 16-byte

//#define	TIME_MEASURMENT

#ifdef TIME_MEASURMENT
	#define	TIME_INSTRUCTION
	#define	AES_RETTYPE		int
#else
	#define	TIME_INSTRUCTION	/##/
	#define	AES_RETTYPE		void
#endif // TIME_MEASUREMENT



/*
AES_RETTYPE AES_enc128(SW_AUX_BUFFERS *bufs, BOOLEAN AES_NI, UCHAR *plainText, UCHAR *cipherText, size_t pSize, UCHAR *xored_key);

AES_RETTYPE AES_enc128CreateKeySched(SW_AUX_BUFFERS *bufs, BOOLEAN AES_NI, UCHAR *xored_key);
AES_RETTYPE AES_enc128FromKeySched(SW_AUX_BUFFERS *bufs, BOOLEAN AES_NI, UCHAR *plainText, UCHAR *cipherText);
AES_RETTYPE AES_enc128DestructKeySched(SW_AUX_BUFFERS *bufs, BOOLEAN AES_NI);

BOOLEAN check_for_aes_instructions(void);
//void __cdecl TPmemset(void * _Dst, int _Val, _In_ size_t _Size) ;
//int __cdecl TPmemcmp(const void * _Buf1, const void * _Buf2, size_t _Size);
 * */

