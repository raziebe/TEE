//#pragma once
#ifndef __AESENCSW_H__
#define __AESENCSW_H__

#include "tp_types.h"
/*
#define	AES128BlockSize		16
#define	AES128KeyRounds		10

enum { ECB = 0, CBC = 1, CFB = 2 };
enum { DEFAULT_BLOCK_SIZE = 16 };
enum { MAX_BLOCK_SIZE = 32, MAX_ROUNDS = 14, MAX_KC = 8, MAX_BC = 8 };

//#pragma warning(disable:4201)

typedef union
{
	struct {
		//Encryption (m_Ke) round key
		int m_Ke[MAX_ROUNDS+1][MAX_BC];
		//Decryption (m_Kd) round key
		int m_Kd[MAX_ROUNDS+1][MAX_BC];
		//Auxiliary private use buffers
		int tk[MAX_KC];
		int a[MAX_BC];
		int t[MAX_BC];
	};
	UINT64 keyStorage[9 * 2];
} SW_AUX_BUFFERS;
*/
#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // H_HH
