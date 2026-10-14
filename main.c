/*
 * Copyright (c) 2020 Graphene
 */

#include <psp2kern/kernel/modulemgr.h>
#include <psp2kern/kernel/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/io/fcntl.h>
#include <taihen.h>
#include <stdarg.h>
#include <stdint.h>

#include "capu_logic.h"

/* Exported by SceSysrootForDriver; VitaSDK has the stub but no prototype. */
int ksceKernelSysrootGetSystemSwVersion(void);
/* Exported by taihenModuleUtils; no prototype in taihen.h. */
int module_get_offset(SceUID pid, SceUID modid, int segidx, size_t offset, uintptr_t *addr);

#define LOG_FILE "ur0:data/capunlocker_log.txt"
#define LOG(...) capu_log(__VA_ARGS__)

/* Boot-time lines, also written to LOG_FILE: kernel printf output at boot is easy to miss. */
static char log_buf[2048];
static unsigned log_len;

static void capu_log(const char *fmt, ...)
{
	char line[160];
	va_list ap;

	va_start(ap, fmt);
	int n = vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	if (n < 0)
		return;
	if (n >= (int)sizeof(line))
		n = sizeof(line) - 1;

	ksceKernelPrintf("[CapUnlocker] %s\n", line);
	if (log_len + (unsigned)n + 1 < sizeof(log_buf)) {
		for (int i = 0; i < n; i++)
			log_buf[log_len++] = line[i];
		log_buf[log_len++] = '\n';
	}
}

static void write_log_file(void)
{
	SceUID fd = ksceIoOpen(LOG_FILE, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if (fd < 0) {
		ksceKernelPrintf("[CapUnlocker] cannot write " LOG_FILE " (0x%08X)\n", fd);
		return;
	}
	ksceIoWrite(fd, log_buf, log_len);
	ksceIoClose(fd);
}

/*
 * Offsets into segment 0 of the firmware modules, as found by the original
 * author. The two function hooks are not checked per firmware; the module NIDs
 * and target bytes are logged so a report says which firmware module was hit.
 * The thread-attribute patch checks its bytes first (patch_thread_attr).
 */
#define APPMGR_IS_ALLOWED_TO_MOUNT_OFFSET      0x15D54
#define THREADMGR_IS_ILLEGAL_AFFINITY_OFFSET   0x114C
#define THREADMGR_ATTR_CHECK_OFFSET_FW_LE_361  0x6222 /* firmware <= 3.61 */
#define THREADMGR_ATTR_CHECK_OFFSET_FW_GT_361  0x628A /* firmware  > 3.61 */
#define THREADMGR_ATTR_CHECK_BRANCH_IMM        0x0D
#define SYSCLIB_STRCMP_NID                     0x0B33BC43

enum {
	HOOK_MOUNT,
	HOOK_AFFINITY,
	HOOK_VM_STRCMP,
	HOOK_COUNT
};

static tai_hook_ref_t hook_ref[HOOK_COUNT];
static SceUID hook_id[HOOK_COUNT] = { -1, -1, -1 };
static SceUID attr_inject_id = -1;

static int isAllowedToMount_patched(int a1)
{
	return 1;
}

static int isIllegalAffinity_patched(int a1, int a2, int a3)
{
	return 0;
}

/*
 * Only imported by SceKernelThreadMgr's VM capability checks (ForVM / ForMono),
 * so reporting every comparison as equal passes those checks.
 */
static int strcmp_patched(const char *str1, const char *str2)
{
	return 0;
}

static void read_config(capu_config *cfg)
{
	/* Same precedence as taiHEN's own config.txt: ux0:tai first, then ur0:tai. */
	static const char *const paths[] = { "ux0:tai/capunlocker.txt", "ur0:tai/capunlocker.txt" };
	static char buf[512];

	capu_config_defaults(cfg);
	for (unsigned i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
		SceUID fd = ksceIoOpen(paths[i], SCE_O_RDONLY, 0);
		if (fd < 0)
			continue;
		int len = ksceIoRead(fd, buf, sizeof(buf) - 1);
		ksceIoClose(fd);
		if (len < 0) {
			LOG("cannot read %s (0x%08X), using defaults", paths[i], len);
			return;
		}
		int applied = capu_config_parse(cfg, buf, (size_t)len);
		LOG("config %s: %d setting(s) applied", paths[i], applied);
		return;
	}
	LOG("no capunlocker.txt, all unlocks enabled");
}

/* Log the first bytes of a patch target so firmware differences can be reported. */
static void log_target(const char *what, SceUID modid, uint32_t offset)
{
	uintptr_t addr;
	if (module_get_offset(KERNEL_PID, modid, 0, offset, &addr) < 0) {
		LOG("%s: offset 0x%X not in module", what, offset);
		return;
	}
	const unsigned char *p = (const unsigned char *)addr;
	LOG("%s: +0x%X bytes %02X %02X %02X %02X", what, offset, p[0], p[1], p[2], p[3]);
}

static int get_module(const char *name, tai_module_info_t *info)
{
	info->size = sizeof(*info);
	int ret = taiGetModuleInfoForKernel(KERNEL_PID, name, info);
	if (ret < 0)
		LOG("%s not found (0x%08X)", name, ret);
	else
		LOG("%s module NID 0x%08X", name, info->module_nid);
	return ret;
}

static SceUID hook_offset(int slot, const char *what, SceUID modid, uint32_t offset, const void *func)
{
	log_target(what, modid, offset);
	hook_id[slot] = taiHookFunctionOffsetForKernel(KERNEL_PID, &hook_ref[slot], modid, 0, offset, 1, func);
	LOG("%s: %s (0x%08X)", what, hook_id[slot] >= 0 ? "hooked" : "hook failed", hook_id[slot]);
	return hook_id[slot];
}

/*
 * The thread-attribute check ends in a conditional branch "B<cond> +0x1A"
 * (bytes 0D Dx). Writing 0D E0 turns it into an unconditional branch to the
 * same target. Patch only if that instruction is really there, so a firmware
 * with a different layout is left untouched instead of being corrupted.
 */
static void patch_thread_attr(SceUID modid)
{
	int sw = ksceKernelSysrootGetSystemSwVersion();
	uint32_t offset = sw > 0x03610000 ? THREADMGR_ATTR_CHECK_OFFSET_FW_GT_361 : THREADMGR_ATTR_CHECK_OFFSET_FW_LE_361;
	uintptr_t addr;

	LOG("system software 0x%08X, thread attr check at +0x%X", sw, offset);
	if (module_get_offset(KERNEL_PID, modid, 0, offset, &addr) < 0) {
		LOG("thread_attr: offset not in module, skipped");
		return;
	}

	const unsigned char *p = (const unsigned char *)addr;
	if (capu_is_thumb_uncond_branch(p, THREADMGR_ATTR_CHECK_BRANCH_IMM)) {
		LOG("thread_attr: already patched, skipped");
		return;
	}
	if (!capu_is_thumb_cond_branch(p, THREADMGR_ATTR_CHECK_BRANCH_IMM)) {
		LOG("thread_attr: unexpected bytes %02X %02X, skipped (unknown firmware layout)", p[0], p[1]);
		return;
	}

	attr_inject_id = taiInjectDataForKernel(KERNEL_PID, modid, 0, offset, "\x0D\xE0", 2);
	LOG("thread_attr: %s (0x%08X)", attr_inject_id >= 0 ? "patched" : "patch failed", attr_inject_id);
}

static int applied_count(void)
{
	int n = attr_inject_id >= 0;
	for (int i = 0; i < HOOK_COUNT; i++)
		n += hook_id[i] >= 0;
	return n;
}

int module_start(SceSize argc, const void *args);
int _start(SceSize argc, const void *args) __attribute__ ((weak, alias("module_start")));
int module_start(SceSize argc, const void *args)
{
	capu_config cfg;
	tai_module_info_t info;

	LOG("v1.5 starting");
	read_config(&cfg);
	LOG("mount=%d core=%d vm=%d thread_attr=%d log=%d", cfg.mount, cfg.core, cfg.vm, cfg.thread_attr, cfg.log);

	if (cfg.mount && get_module("SceAppMgr", &info) >= 0)
		hook_offset(HOOK_MOUNT, "mount", info.modid, APPMGR_IS_ALLOWED_TO_MOUNT_OFFSET, isAllowedToMount_patched);

	if ((cfg.core || cfg.vm || cfg.thread_attr) && get_module("SceKernelThreadMgr", &info) >= 0) {
		if (cfg.core)
			hook_offset(HOOK_AFFINITY, "core", info.modid, THREADMGR_IS_ILLEGAL_AFFINITY_OFFSET, isIllegalAffinity_patched);

		if (cfg.vm) {
			hook_id[HOOK_VM_STRCMP] = taiHookFunctionImportForKernel(KERNEL_PID, &hook_ref[HOOK_VM_STRCMP],
				"SceKernelThreadMgr", TAI_ANY_LIBRARY, SYSCLIB_STRCMP_NID, strcmp_patched);
			LOG("vm: %s (0x%08X)", hook_id[HOOK_VM_STRCMP] >= 0 ? "hooked" : "hook failed", hook_id[HOOK_VM_STRCMP]);
		}

		if (cfg.thread_attr)
			patch_thread_attr(info.modid);
	}

	int n = applied_count();
	LOG("%d patch(es) active", n);
	if (cfg.log)
		write_log_file();
	/* Nothing applied: unload instead of staying resident. */
	return n > 0 ? SCE_KERNEL_START_SUCCESS : SCE_KERNEL_START_NO_RESIDENT;
}

int module_stop(SceSize argc, const void *args)
{
	if (attr_inject_id >= 0)
		taiInjectReleaseForKernel(attr_inject_id);
	for (int i = 0; i < HOOK_COUNT; i++) {
		if (hook_id[i] >= 0)
			taiHookReleaseForKernel(hook_id[i], hook_ref[i]);
	}
	return SCE_KERNEL_STOP_SUCCESS;
}
