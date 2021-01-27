/*
 * Copyright (c) 2020 Graphene
 */

#include <psp2kern/kernel/modulemgr.h>
#include <taihen.h>

static tai_hook_ref_t hook_ref[3];
static SceUID hook_id[3];

static int isAllowedToMount_patched(int a1)
{
	return 1;
}

static int isIllegalAffinity_patched(int a1, int a2, int a3)
{
	return 0;
}

static int isAllowedVMFunctions_patched(int a1)
{
	return 1;
}

void _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize argc, const void *args)
{
	tai_module_info_t info;
	info.size = sizeof(tai_module_info_t);
	taiGetModuleInfoForKernel(KERNEL_PID, "SceAppMgr", &info);

	hook_id[0] = taiHookFunctionOffsetForKernel(
		KERNEL_PID, 
		&hook_ref[0], 
		info.modid, 
		0, 
		0x15d54,
		1, 
		isAllowedToMount_patched);

	taiGetModuleInfoForKernel(KERNEL_PID, "SceKernelThreadMgr", &info);

	hook_id[1] = taiHookFunctionOffsetForKernel(
		KERNEL_PID,
		&hook_ref[1],
		info.modid,
		0,
		0x114C,
		1,
		isIllegalAffinity_patched);

	hook_id[2] = taiHookFunctionOffsetForKernel(
		KERNEL_PID,
		&hook_ref[2],
		info.modid,
		0,
		0x28764,
		1,
		isAllowedVMFunctions_patched);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	if(hook_id[0] >= 0) taiHookReleaseForKernel(hook_id[0], hook_ref[0]);
	if(hook_id[1] >= 0) taiHookReleaseForKernel(hook_id[1], hook_ref[1]);
	if(hook_id[2] >= 0) taiHookReleaseForKernel(hook_id[2], hook_ref[2]);
	return SCE_KERNEL_STOP_SUCCESS;
}
