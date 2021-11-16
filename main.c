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

static int strcmp_patched(const char *str1, const char *str2)
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

	// We can just return 0 since it is only used in VM cap check functions
	hook_id[2] = taiHookFunctionImportForKernel(
		KERNEL_PID,
		&hook_ref[2],
		"SceKernelThreadMgr",
		TAI_ANY_LIBRARY,
		0x0B33BC43,
		strcmp_patched);

	//Fixing Henkaku sins...
	int swVer = sceKernelSysrootGetSystemSwVersion();

	if (swVer > 0x03610000) {
		hook_id[3] = taiInjectDataForKernel(KERNEL_PID, info.modid, 0, 0x628A, "\x0D\xE0", 2);
	}
	else {
		hook_id[3] = taiInjectDataForKernel(KERNEL_PID, info.modid, 0, 0x6222, "\x0D\xE0", 2);
	}

	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args)
{
	if(hook_id[0] >= 0) taiHookReleaseForKernel(hook_id[0], hook_ref[0]);
	if(hook_id[1] >= 0) taiHookReleaseForKernel(hook_id[1], hook_ref[1]);
	if(hook_id[2] >= 0) taiHookReleaseForKernel(hook_id[2], hook_ref[2]);
	if(hook_id[3] >= 0) taiInjectReleaseForKernel(hook_id[3]);
	return SCE_KERNEL_STOP_SUCCESS;
}
