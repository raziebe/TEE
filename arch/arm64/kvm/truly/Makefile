#
# Makefile for Truly
#

ccflags-y += -Iarch/arm64/kvm
CFLAGS_arm.o := -I.
CFLAGS_mmu.o := -I.

KVM=../../../virt/kvm
ARM=../../../arch/arm/kvm

obj-$(CONFIG_TRULY_PROTECT) += truly.o trulyS.o
