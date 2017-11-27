/*
 * tp_mmu.h
 *
 *  Created on: Nov 9, 2017
 *      Author: raz
 */

#ifndef INCLUDE_LINUX_TP_MMU_H_
#define INCLUDE_LINUX_TP_MMU_H_

struct _IMAGE_FILE;

void tp_map_vmas(struct _IMAGE_FILE* image_file);
void vma_map_hyp(struct vm_area_struct* vma);
void tp_unmmap_handler(struct task_struct* task);
void tp_mmap_handler(unsigned long addr,int len);
void tp_reset_tvm(void);
void tp_unmmap_handler(struct task_struct* task);
void map_user_space_data(void *umem,int size);
void hyp_user_unmap(unsigned long umem,int size,int user);
int create_hyp_mappings(void *, void *);
int create_hyp_user_mappings(void *,void*);
void make_vtcr_el2(struct truly_vm *tvm);
void tp_create_pg_tbl(void *cxt);
unsigned long kvm_uaddr_to_pfn(unsigned long uaddr);

#define USER_TO_HYP(uva)	(uva)

#endif /* INCLUDE_LINUX_TP_MMU_H_ */
