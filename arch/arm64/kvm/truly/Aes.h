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

void __hyp_text AESSW_Dec128(struct encrypt_tvm *, const unsigned __int8 *in,
	unsigned __int8 *result, const unsigned __int8 *key, size_t numBlocks);

void __hyp_text AESSW_Enc128(struct encrypt_tvm *, const unsigned __int8 *in,
	unsigned __int8 *result, size_t numBlocks, const unsigned __int8 *key);

void EncryptInit(struct truly_vm *tvm);

