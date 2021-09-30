/*
 * Copyright (c) 2020 Graphene
 */

#include <kernel.h>
#include <taihen.h>

static tai_hook_ref_t hook_ref[5];
static SceUID hook_id[5];

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

static int sceSblACMgrIsRootProgram_patched(SceUID pid)
{
	return 0;
}

static int sceSblACMgrIsSystemProgram_patched(SceUID pid)
{
	return 0;
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

	//Fixing Henkaku sins...
	hook_id[3] = taiHookFunctionImportForKernel(
		KERNEL_PID,
		&hook_ref[3],
		"SceKernelThreadMgr",
		TAI_ANY_LIBRARY,
		0x31C23B66,
		sceSblACMgrIsRootProgram_patched);

	hook_id[4] = taiHookFunctionImportForKernel(
		KERNEL_PID,
		&hook_ref[4],
		"SceKernelThreadMgr",
		TAI_ANY_LIBRARY,
		0x930CD037,
		sceSblACMgrIsSystemProgram_patched);

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	if(hook_id[0] >= 0) taiHookReleaseForKernel(hook_id[0], hook_ref[0]);
	if(hook_id[1] >= 0) taiHookReleaseForKernel(hook_id[1], hook_ref[1]);
	if(hook_id[2] >= 0) taiHookReleaseForKernel(hook_id[2], hook_ref[2]);
	if(hook_id[3] >= 0) taiHookReleaseForKernel(hook_id[3], hook_ref[3]);
	if(hook_id[4] >= 0) taiHookReleaseForKernel(hook_id[4], hook_ref[4]);
	return SCE_KERNEL_STOP_SUCCESS;
}
