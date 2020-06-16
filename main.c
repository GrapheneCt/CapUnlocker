/*
 * Copyright (c) 2020 Graphene
 */

#include <psp2kern/kernel/modulemgr.h>
#include <taihen.h>

static tai_hook_ref_t hook_ref;
static SceUID hook_id;

// yes, it is really that easy
static int isIllegalAffinity_patched(int a1, int a2, int a3)
{
	return 0;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize argc, const void *args)
{
	tai_module_info_t info;
	info.size = sizeof(tai_module_info_t);
	taiGetModuleInfoForKernel(KERNEL_PID, "SceKernelThreadMgr", &info);

	hook_id = taiHookFunctionOffsetForKernel(
		KERNEL_PID, 
		&hook_ref, 
		info.modid, 
		0, 
		0x114C, 
		1, 
		isIllegalAffinity_patched);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	if(hook_id >= 0) taiHookReleaseForKernel(hook_id, hook_ref);
	return SCE_KERNEL_STOP_SUCCESS;
}
