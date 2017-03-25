#pragma once

//#include "Vmx.h"

//extern Module aesModule;

void AesDecrypt(void *input, void *output, size_t length, void *xoredKey);
void AesCreateKeySched(void *xoredKey);
void AesFromKeySched(void *input, void *output);
void AesDestructKeySched(void);
