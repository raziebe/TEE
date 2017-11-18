#include <linux/module.h>
#include <linux/truly.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/gfp.h>
#include <linux/highmem.h>
#include <linux/compiler.h>
#include <linux/linkage.h>
#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/sections.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <linux/truly.h>
#include <linux/delay.h>
#include "Aes.h"
#include "ImageFile.h"

DECLARE_PER_CPU(struct truly_vm, TVM);



int __hyp_text truly_is_protected(struct truly_vm *tv)
{
	if (tv == NULL)
		tv = get_tvm();
	return tv->protected_pgd == truly_get_ttbr0_el1();
}

void tp_reset_tvm(void)
{
	int cpu;
	for_each_possible_cpu(cpu) {
			struct truly_vm *tv = get_tvm_per_cpu(cpu);
			tv->protected_pgd = 0;
			tv->tv_flags = 0;
			tv->elr_el2  = 0;
			tv->first_lr = 0;
			tv->elr_el2  = 0;
			tv->esr_el2  = 0;
			tv->spsr_el2  = 0;
			tv->brk_count_el2 = 0;
	}
}

#include "AesC.h"

int __hyp_text truly_decrypt(struct truly_vm *tv)
{
	int extra;
	int extra_offset;
	char extra_lines[16];
	int line = 0,lines = 0;
	unsigned char *d = NULL;
	unsigned char *pad = NULL;
	int data_offset = 60;
	UCHAR key[16+1] = {0};
	struct encrypt_tvm *enc;
	long t1;

	enc = (struct encrypt_tvm *) KERN_TO_HYP(tv->enc);

	if (tv->protected_pgd != truly_get_ttbr0_el1()) {
		return CODE_ERROR;
	}

	if (!(tv->tv_flags & TVM_SHOULD_DECRYPT)) {
		tv->brk_count_el2++;
		t1 = cycles();
		tp_hyp_memcpy( enc->seg[0].pad_data ,
					(unsigned char *)KERN_TO_HYP( enc->seg[0].decrypted_data ) , enc->seg[0].size);
		tv->copy_time  = cycles() - t1;
		return CODE_COPIED;
	}

	tv->tv_flags &=  ~TVM_SHOULD_DECRYPT;

	if (enc->seg[0].pad_data == NULL) {
		enc->seg[0].pad_data = (char *)tv->elr_el2;
	}

	pad = enc->seg[0].pad_data;
	tv->brk_count_el2++;
	get_decrypted_key(key);
	lines = enc->seg[0].size/ 4;

	// AES is in a 16 bytes blocks
	extra_offset = (enc->seg[0].size/ 16) * 16;
	extra = enc->seg[0].size - extra_offset;

	if ( extra > 0) {
		// backup extra code
		tp_hyp_memcpy(extra_lines, pad +  enc->seg[0].size, sizeof(extra_lines) - extra);
		// pad with zeros
		tp_hyp_memset(pad + enc->seg[0].size ,(char)0, sizeof(extra_lines) - extra );
		lines++;
	}

	d = (char *)KERN_TO_HYP(enc->seg[0].enc_data);
	d += data_offset;

	t1 = cycles();
	for (line = 0 ; line < lines ; line += 4 ) {
		AESSW_Enc128( enc, d , pad, 1 ,key);
		d   += 16;
		pad += 16;
	}
	tv->decrypt_time  = cycles() - t1;

	if (extra > 0) {
		pad = enc->seg[0].pad_data;
		tp_hyp_memcpy( pad +  enc->seg[0].size, extra_lines ,sizeof(extra_lines) - extra);
	}
	/*
	* Pad is filled with the encrypted data ;back it up to the decrypted_data
	*/
	tp_hyp_memcpy( (unsigned char *)KERN_TO_HYP(enc->seg[0].decrypted_data) ,
				enc->seg[0].pad_data  , enc->seg[0].size);
	return CODE_DECRYPTED;
}


int __hyp_text truly_pad(struct truly_vm *tv)
{
	char fault_cmd[4];
	int line = 0,lines = 0;
	unsigned char *pad;
	struct encrypt_tvm *enc;
	long t1;

	t1 = cycles();
	enc = (struct encrypt_tvm *) KERN_TO_HYP(tv->enc);

	pad = enc->seg[0].pad_data;
	lines = enc->seg[0].size / 4;

	/*
	* if the fault was in the padded function
	* we must put back the the command that generated the fault
	* ( for example svc ) and re commence it.
	*/
	if (tv->save_cmd < (unsigned long)pad){
		tv->save_cmd = 0;
	}

	if (tv->save_cmd > ( (unsigned long)pad + enc->seg[0].size) ){
		tv->save_cmd = 0;
	}

	if (tv->save_cmd != 0)
		tp_hyp_memcpy(fault_cmd, (char *)tv->save_cmd, sizeof(fault_cmd));

	for (line = 0 ; line < lines ; line++ ) {
		int* p = (int*)&pad[4*line];
		*p  = 0xd4200060;
	}
	tv->pad_time = cycles() - t1;
	if (!tv->save_cmd)
		return 0xA;
	//
	// Must put back the old command
	//
	tp_hyp_memcpy((char *)tv->save_cmd, fault_cmd, sizeof(fault_cmd));

	return 0xB;
}
