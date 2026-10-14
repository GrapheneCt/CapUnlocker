/*
 * Copyright (c) 2020 Graphene
 *
 * Pure helpers with no kernel dependencies, so they can be unit-tested on a
 * host machine (see tests/).
 */

#ifndef CAPU_LOGIC_H
#define CAPU_LOGIC_H

#include <stddef.h>

typedef struct capu_config {
	int mount;       /* SceAppMgr: allow mounting virtual drives such as cache0: */
	int core;        /* SceKernelThreadMgr: allow the 4th CPU core in thread affinity */
	int vm;          /* SceKernelThreadMgr: allow the ForVM / ForMono thread functions */
	int thread_attr; /* SceKernelThreadMgr: allow all thread attributes */
	int log;         /* write ur0:data/capunlocker_log.txt at boot */
} capu_config;

/* Every unlock enabled, which is what CapUnlocker 1.4 always did, and the log file on. */
void capu_config_defaults(capu_config *cfg);

/*
 * Parse "key = value" lines. '#' starts a comment. Values: 1/0, on/off,
 * true/false, yes/no (any case). Unknown keys and bad values are ignored and
 * leave the current setting unchanged.
 * Returns the number of settings applied.
 */
int capu_config_parse(capu_config *cfg, const char *buf, size_t len);

/* Thumb T1 conditional branch "B<cond> #imm": bytes imm8, 0xD0 | cond (cond 0..13). */
int capu_is_thumb_cond_branch(const unsigned char *insn, unsigned char imm8);

/* Thumb T2 unconditional branch "B #imm" whose imm11 fits in one byte: bytes imm8, 0xE0. */
int capu_is_thumb_uncond_branch(const unsigned char *insn, unsigned char imm8);

#endif
