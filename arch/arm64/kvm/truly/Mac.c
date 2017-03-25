#include "Mac.h"
#include "common.h"
#include "AESencAPI.h"
//#include "hypervisor.h"
#include "Aes.h"

void BYTE_ARRAY_ShiftL1bit(UCHAR dest[], UCHAR source[])
{
	unsigned char overflow = 0;
	int i;
	for (i = 15; i >= 0; i--)
	{
		unsigned char new_overflow = (source[i] & 0x80) ? 1 : 0;
		dest[i] = source[i] << 1;
		dest[i] |= overflow;
		overflow = new_overflow;
	}
}

void GenerateSubKey(UCHAR inputKey[], UCHAR generatedKey[])
{
	BOOLEAN msbIsOne = inputKey[0] >> 7 == 1;
	BYTE_ARRAY_ShiftL1bit(generatedKey, inputKey);
	generatedKey[MAC_TAG_SIZE - 1] ^= msbIsOne * 0x87;
}

#define XOR(X, Y) do { ((__int64*)X)[0] ^= ((__int64*)Y)[0]; ((__int64*)X)[1] ^= ((__int64*)Y)[1];} while ((void)0,0)

void mac_compute_tag(SW_AUX_BUFFERS *bufs, BOOLEAN AES_NI, UCHAR *code, UINT32 length, UCHAR *key, UCHAR *mac)
{
	UCHAR k[MAC_TAG_SIZE] = { 0 }, block[MAC_TAG_SIZE] = { 0 };

	//SIMPLE_MEASURE_BEGIN
	AES_enc128CreateKeySched(bufs, AES_NI, key);
	//SIMPLE_MEASURE_END
	// encrypt all but last block
	//SIMPLE_MEASURE_BEGIN
	for (; length > MAC_TAG_SIZE; code += MAC_TAG_SIZE, length -= MAC_TAG_SIZE)
	{
		XOR(block, code);
		AES_enc128FromKeySched(bufs, AES_NI, block, block);
	}
	//SIMPLE_MEASURE_END

	// compute the subkey k and incorporate it
	//SIMPLE_MEASURE_BEGIN
	AES_enc128FromKeySched(bufs, AES_NI, k, k);
	GenerateSubKey(k, k);
	if (length != MAC_TAG_SIZE)
		GenerateSubKey(k, k);
	XOR(block, k);
	//SIMPLE_MEASURE_END

	//SIMPLE_MEASURE_BEGIN
	// incorporate the padded last sub-block
	block[length] ^= (length != MAC_TAG_SIZE) * 0x80;
	for (; length; --length)
		block[length - 1] ^= code[length - 1];

	// encrypt once again to compute mac
	AES_enc128FromKeySched(bufs, AES_NI, block, mac);
	//SIMPLE_MEASURE_END

	//SIMPLE_MEASURE_BEGIN
	AES_enc128DestructKeySched(bufs, AES_NI);
	//SIMPLE_MEASURE_END
}


void MacComputeTag(void *input, size_t length, void *xoredKey, void *mac)
{
	UCHAR k[MAC_TAG_SIZE] = { 0 }, block[MAC_TAG_SIZE] = { 0 }, *code = (UCHAR*)input;

	AesCreateKeySched(xoredKey);

	// encrypt all but last block
	for (; length > MAC_TAG_SIZE; code += MAC_TAG_SIZE, length -= MAC_TAG_SIZE)
	{
		XOR(block, code);
		AesFromKeySched(block, block);
	}

	// compute the subkey k and incorporate it
	AesFromKeySched(k, k);
	GenerateSubKey(k, k);
	if (length != MAC_TAG_SIZE)
		GenerateSubKey(k, k);
	XOR(block, k);

	// incorporate the padded last sub-block
	block[length] ^= (length != MAC_TAG_SIZE) * 0x80;
	for (; length; --length)
		block[length - 1] ^= code[length - 1];

	// encrypt once again to compute mac
	AesFromKeySched(block, mac);

	AesDestructKeySched();
}
