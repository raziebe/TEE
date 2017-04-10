#ifndef __MAC_H__
#define __MAC_H__

#include "tp_types.h"
#include "AESencSW.h"

#define MAC_TAG_SIZE 16
/*
void mac_compute_tag(SW_AUX_BUFFERS *bufs, BOOLEAN AES_NI,
	UCHAR *encryptedCode,
	UINT32 encrytCodeLength,
	UCHAR *key,
	UCHAR *mac);

void MacComputeTag(void *input, size_t length, void *xoredKey, void *mac);
*/
#endif
