
#ifndef ARCH_ARM64_KVM_TRULY_AESC_H_
#define ARCH_ARM64_KVM_TRULY_AESC_H_



//Auxiliary Function
static inline void __hyp_text Xor(char* buff, char const* chain)
{
	int i;
	for(i = 0; i < AES128BlockSize ; i++)
		*(buff++) ^= *(chain++);
}


//Expand a user-supplied key material into a session key.
// key        - The 128-bit user-key to use.
static inline void __hyp_text  MakeKey(struct encrypt_tvm* tvm, const UCHAR *key)
{
	const UCHAR *pc;
	int* pi;
	int t = 0;
	int i, j, r;
	int tt, rconpointer = 0;
	int ROUND_KEY_COUNT = (AES128KeyRounds + 1) * 4;

	for(i = 0; i <= AES128KeyRounds; i++)
	{
		for(j=0; j<4; j++)
			tvm->m_Ke[i][j] = 0;
	}

	for(i=0; i <= AES128KeyRounds; i++)
	{
		for(j=0; j < 4; j++)
			tvm->m_Kd[i][j] = 0;
	}


	//Copy user material bytes into temporary ints
	pi = tvm->tk;
	pc = key;
	for(i = 0; i < 4; i++)
	{
		*pi = (unsigned char)*(pc++) << 24;
		*pi |= (unsigned char)*(pc++) << 16;
		*pi |= (unsigned char)*(pc++) << 8;
		*(pi++) |= (unsigned char)*(pc++);
	}
	//Copy values into round key arrays

	for(j=0; (j<4)&&(t<ROUND_KEY_COUNT); j++,t++)
	{
		tvm->m_Ke[t/4][t%4] = tvm->tk[j];
		tvm->m_Kd[AES128KeyRounds - (t/4)][t%4] = tvm->tk[j];
	}

	while(t < ROUND_KEY_COUNT)
	{
		//Extrapolate using phi (the round key evolution function)
		tt = tvm->tk[4-1];
		tvm->tk[0] ^= (tvm->sm_S[(tt >> 16) & 0xFF] & 0xFF) << 24 ^
		 	 (tvm->sm_S[(tt >>  8) & 0xFF] & 0xFF) << 16 ^
			 (tvm->sm_S[ tt & 0xFF] & 0xFF) <<  8 ^
			 (tvm->sm_S[(tt >> 24) & 0xFF] & 0xFF) ^
			 (tvm->sm_rcon[rconpointer++]  & 0xFF) << 24;
		for(i=1, j = 0; i<4;)
			tvm->tk[i++] ^= tvm->tk[j++];
		//Copy values into round key arrays
		for(j=0; (j<4) && (t<ROUND_KEY_COUNT); j++, t++)
		{
			tvm->m_Ke[t/4][t%4] = tvm->tk[j];
			tvm->m_Kd[AES128KeyRounds - (t/4)][t%4] = tvm->tk[j];
		}
	}
	//Inverse MixColumn where needed
	for(r = 1; r < AES128KeyRounds; r++)
		for(j=0; j<4; j++)
		{
			tt = tvm->m_Kd[r][j];
			tvm->m_Kd[r][j] = tvm->sm_U1[(tt >> 24) & 0xFF] ^
				tvm->sm_U2[(tt >> 16) & 0xFF] ^
				tvm->sm_U3[(tt >>  8) & 0xFF] ^
				tvm->sm_U4[tt & 0xFF];
		}
}


//Convenience method to encrypt exactly one block of plaintext
// in         - The plaintext
// result     - The ciphertext generated from a plaintext using the key
static inline void __hyp_text  DefEncryptBlock(struct encrypt_tvm *tvm, const UCHAR *in, UCHAR *result)
{
	int a0, a1, a2, a3;
	int t0;
	int t1;
	int t2;
	int t3;
	int r;
	int tt;
	int* Ker = tvm->m_Ke[0];

	t0 = ((unsigned char)*(in++) << 24);
	t0 |= ((unsigned char)*(in++) << 16);
	t0 |= ((unsigned char)*(in++) << 8);
	t0 |= (unsigned char)*(in++);
	t0 ^= Ker[0];

	t1 = ((unsigned char)*(in++) << 24);
	t1 |= ((unsigned char)*(in++) << 16);
	t1 |= ((unsigned char)*(in++) << 8);
	t1 |= (unsigned char)*(in++);
	t1 ^= Ker[1];

	t2 = ((unsigned char)*(in++) << 24);
	t2 |= ((unsigned char)*(in++) << 16);
	t2 |= ((unsigned char)*(in++) << 8);
	(t2 |= (unsigned char)*(in++));
	t2 ^= Ker[2];

	t3 = ((unsigned char)*(in++) << 24);
	t3 |= ((unsigned char)*(in++) << 16);
	t3 |= ((unsigned char)*(in++) << 8);
	(t3 |= (unsigned char)*(in++));
	t3 ^= Ker[3];

	//Apply Round Transforms
	for (r = 1; r < AES128KeyRounds; r++)
	{
		Ker = tvm->m_Ke[r];

		a0 = (tvm->sm_T1[(t0 >> 24) & 0xFF] ^
			tvm->sm_T2[(t1 >> 16) & 0xFF] ^
			tvm->sm_T3[(t2 >>  8) & 0xFF] ^
			tvm->sm_T4[t3 & 0xFF]) ^ Ker[0];

		a1 = ( tvm->sm_T1[(t1 >> 24) & 0xFF] ^
			tvm->sm_T2[(t2 >> 16) & 0xFF] ^
			tvm->sm_T3[(t3 >>  8) & 0xFF] ^
			tvm->sm_T4[t0 & 0xFF]) ^ Ker[1];

		a2 = ( tvm->sm_T1[(t2 >> 24) & 0xFF] ^
			tvm->sm_T2[(t3 >> 16) & 0xFF] ^
			tvm->sm_T3[(t0 >>  8) & 0xFF] ^
			tvm->sm_T4[t1 & 0xFF]) ^ Ker[2];

		a3 = ( tvm->sm_T1[(t3 >> 24) & 0xFF] ^
			tvm->sm_T2[(t0 >> 16) & 0xFF] ^
			tvm->sm_T3[(t1 >>  8) & 0xFF] ^
			tvm->sm_T4[t2 & 0xFF]) ^ Ker[3];

		t0 = a0;
		t1 = a1;
		t2 = a2;
		t3 = a3;
	}
	//Last Round is special
	Ker = tvm->m_Ke[AES128KeyRounds];
	tt = Ker[0];
	result[0] = tvm->sm_S[(t0 >> 24) & 0xFF] ^ (tt >> 24);
	result[1] = tvm->sm_S[(t1 >> 16) & 0xFF] ^ (tt >> 16);
	result[2] = tvm->sm_S[(t2 >>  8) & 0xFF] ^ (tt >>  8);
	result[3] = tvm->sm_S[t3 & 0xFF] ^ tt;
	tt = Ker[1];
	result[4] = tvm->sm_S[(t1 >> 24) & 0xFF] ^ (tt >> 24);
	result[5] = tvm->sm_S[(t2 >> 16) & 0xFF] ^ (tt >> 16);
	result[6] = tvm->sm_S[(t3 >>  8) & 0xFF] ^ (tt >>  8);
	result[7] = tvm->sm_S[t0 & 0xFF] ^ tt;
	tt = Ker[2];
	result[8] = tvm->sm_S[(t2 >> 24) & 0xFF] ^ (tt >> 24);
	result[9] = tvm->sm_S[(t3 >> 16) & 0xFF] ^ (tt >> 16);
	result[10] = tvm->sm_S[(t0 >>  8) & 0xFF] ^ (tt >>  8);
	result[11] = tvm->sm_S[t1 & 0xFF] ^ tt;
	tt = Ker[3];
	result[12] = tvm->sm_S[(t3 >> 24) & 0xFF] ^ (tt >> 24);
	result[13] = tvm->sm_S[(t0 >> 16) & 0xFF] ^ (tt >> 16);
	result[14] = tvm->sm_S[(t1 >>  8) & 0xFF] ^ (tt >>  8);
	result[15] = tvm->sm_S[t2 & 0xFF] ^ tt;
}

//Convenience method to decrypt exactly one block of plaintext,
// in         - The ciphertext.
// result     - The plaintext generated from a ciphertext using the session key.
static inline void __hyp_text  DefDecryptBlock(struct encrypt_tvm *tvm, const UCHAR *in, UCHAR *result)
{
	int a0, a1, a2, a3;
	int t0, t1, t2, t3;
	int r;
	int tt;

	int* Kdr = tvm->m_Kd[0];

	t0 = ((unsigned char)*(in++) << 24);
	t0 = t0 | ((unsigned char)*(in++) << 16);
	t0 |= ((unsigned char)*(in++) << 8);
	(t0 |= (unsigned char)*(in++));
	t0  ^= Kdr[0];

	t1 = ((unsigned char)*(in++) << 24);
	t1 |= ((unsigned char)*(in++) << 16);
	t1 |= ((unsigned char)*(in++) << 8);
	(t1 |= (unsigned char)*(in++));
	t1 ^= Kdr[1];

	t2 = ((unsigned char)*(in++) << 24);
	t2 |= ((unsigned char)*(in++) << 16);
	t2 |= ((unsigned char)*(in++) << 8);
	(t2 |= (unsigned char)*(in++));
	t2 ^= Kdr[2];

	t3 = ((unsigned char)*(in++) << 24);
	t3 |= ((unsigned char)*(in++) << 16);
	t3 |= ((unsigned char)*(in++) << 8);
	(t3 |= (unsigned char)*(in++));
	t3 ^= Kdr[3];

	for(r = 1; r < AES128KeyRounds; r++) // apply round transforms
	{
		Kdr = tvm->m_Kd[r];
		a0 = ( tvm-> sm_T5[(t0 >> 24) & 0xFF] ^
			tvm->sm_T6[(t3 >> 16) & 0xFF] ^
			tvm->sm_T7[(t2 >>  8) & 0xFF] ^
			tvm->sm_T8[ t1 & 0xFF] ) ^ Kdr[0];
		a1 = ( tvm->sm_T5[(t1 >> 24) & 0xFF] ^
			tvm->sm_T6[(t0 >> 16) & 0xFF] ^
			tvm->sm_T7[(t3 >>  8) & 0xFF] ^
			tvm->sm_T8[ t2        & 0xFF] ) ^ Kdr[1];
		a2 = ( tvm->sm_T5[(t2 >> 24) & 0xFF] ^
			tvm->sm_T6[(t1 >> 16) & 0xFF] ^
			tvm->sm_T7[(t0 >>  8) & 0xFF] ^
			tvm->sm_T8[ t3        & 0xFF] ) ^ Kdr[2];
		a3 = ( tvm->sm_T5[(t3 >> 24) & 0xFF] ^
			tvm->sm_T6[(t2 >> 16) & 0xFF] ^
			tvm->sm_T7[(t1 >>  8) & 0xFF] ^
			tvm->sm_T8[ t0        & 0xFF] ) ^ Kdr[3];
		t0 = a0;
		t1 = a1;
		t2 = a2;
		t3 = a3;
	}
	//Last Round is special
	Kdr = tvm->m_Kd[AES128KeyRounds];
	tt = Kdr[0];
	result[ 0] = tvm->sm_Si[(t0 >> 24) & 0xFF] ^ (tt >> 24);
	result[ 1] = tvm->sm_Si[(t3 >> 16) & 0xFF] ^ (tt >> 16);
	result[ 2] = tvm->sm_Si[(t2 >>  8) & 0xFF] ^ (tt >>  8);
	result[ 3] = tvm->sm_Si[ t1 & 0xFF] ^ tt;
	tt = Kdr[1];
	result[ 4] = tvm->sm_Si[(t1 >> 24) & 0xFF] ^ (tt >> 24);
	result[ 5] = tvm->sm_Si[(t0 >> 16) & 0xFF] ^ (tt >> 16);
	result[ 6] = tvm->sm_Si[(t3 >>  8) & 0xFF] ^ (tt >>  8);
	result[ 7] = tvm->sm_Si[ t2 & 0xFF] ^ tt;
	tt = Kdr[2];
	result[ 8] = tvm->sm_Si[(t2 >> 24) & 0xFF] ^ (tt >> 24);
	result[ 9] = tvm->sm_Si[(t1 >> 16) & 0xFF] ^ (tt >> 16);
	result[10] = tvm->sm_Si[(t0 >>  8) & 0xFF] ^ (tt >>  8);
	result[11] = tvm->sm_Si[ t3 & 0xFF] ^ tt;
	tt = Kdr[3];
	result[12] = tvm->sm_Si[(t3 >> 24) & 0xFF] ^ (tt >> 24);
	result[13] = tvm->sm_Si[(t2 >> 16) & 0xFF] ^ (tt >> 16);
	result[14] = tvm->sm_Si[(t1 >>  8) & 0xFF] ^ (tt >>  8);
	result[15] = tvm->sm_Si[ t0 & 0xFF] ^ tt;
}



static inline void __hyp_text  AESSW_Enc128(struct encrypt_tvm *tvm,
		const UCHAR *in, UCHAR *result, size_t numBlocks, const UCHAR *key)
{
		size_t i;
		const UCHAR *pin;
		UCHAR *presult;

		MakeKey(tvm, key) ;

		for(i = 0, pin = in, presult =result; i < numBlocks; i++)
		{
			DefEncryptBlock(tvm, pin, presult);
			pin += AES128BlockSize;
			presult += AES128BlockSize;
		}
}


static inline void __hyp_text  AESSW_Dec128(struct encrypt_tvm *tvm,
		const UCHAR *in,
		UCHAR *result,
		const UCHAR *key, size_t numBlocks)
{
		size_t i;
		const UCHAR *pin;
		UCHAR *presult;

		MakeKey(tvm, key);

		for(i = 0, pin = in,presult=result; i < numBlocks ; i++)
		{
			DefDecryptBlock(tvm, pin, presult);
			pin += AES128BlockSize;
			presult += AES128BlockSize;
		}
}

int  __hyp_text  tp_hyp_memcpy(unsigned char *dst,unsigned char *src,int size);
int __hyp_text  tp_hyp_memset(char *dst,char tag,int size);

static inline void get_decrypted_key(UCHAR *key)
{
	UCHAR k[]  = {
			0x2b,0x7e,0x15,0x16,
			0x28,0xae,0xd2,0xa6,
			0xab,0xf7,0x15,0x88,
			0x09,0xcf,0x4f,0x3c};

	tp_hyp_memcpy(key,k, 16);
}



#endif /* ARCH_ARM64_KVM_TRULY_AESC_H_ */
