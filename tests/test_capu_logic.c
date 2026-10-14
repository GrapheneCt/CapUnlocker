/* Host unit tests for capu_logic.c: cc -I.. test_capu_logic.c ../capu_logic.c */

#include <stdio.h>
#include <string.h>

#include "capu_logic.h"

static int failures;

#define CHECK(cond)                                                        \
	do {                                                               \
		if (!(cond)) {                                             \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++;                                        \
		}                                                          \
	} while (0)

static capu_config parse(const char *text, int *applied)
{
	capu_config cfg;
	capu_config_defaults(&cfg);
	*applied = capu_config_parse(&cfg, text, strlen(text));
	return cfg;
}

static void test_defaults(void)
{
	capu_config cfg;
	capu_config_defaults(&cfg);
	CHECK(cfg.mount == 1 && cfg.core == 1 && cfg.vm == 1 && cfg.thread_attr == 1 && cfg.log == 1);
}

static void test_basic(void)
{
	int n;
	capu_config cfg = parse("mount=0\ncore=1\nvm=0\nthread_attr=0\nlog=off\n", &n);
	CHECK(n == 5);
	CHECK(cfg.mount == 0 && cfg.core == 1 && cfg.vm == 0 && cfg.thread_attr == 0 && cfg.log == 0);
}

static void test_spacing_case_comments_crlf(void)
{
	int n;
	capu_config cfg = parse("# CapUnlocker\r\n  Mount = OFF  # no cache0:\r\n\tVM\t=\tNo\r\nthread_attr = yes\r\n", &n);
	CHECK(n == 3);
	CHECK(cfg.mount == 0 && cfg.vm == 0 && cfg.thread_attr == 1 && cfg.core == 1);
}

static void test_bad_lines_ignored(void)
{
	int n;
	capu_config cfg = parse("mount\ncore=maybe\nunknown=0\n=1\nvm=\n# thread_attr=0\nthread_attr==0\n", &n);
	CHECK(n == 0);
	CHECK(cfg.mount == 1 && cfg.core == 1 && cfg.vm == 1 && cfg.thread_attr == 1);
}

static void test_last_value_wins_and_no_trailing_newline(void)
{
	int n;
	capu_config cfg = parse("core=0\ncore=true", &n);
	CHECK(n == 2);
	CHECK(cfg.core == 1);
}

static void test_prefix_keys_rejected(void)
{
	int n;
	capu_config cfg = parse("mounts=0\nvmx=0\nthread=0\n", &n);
	CHECK(n == 0);
	CHECK(cfg.mount == 1 && cfg.vm == 1 && cfg.thread_attr == 1);
}

static void test_empty(void)
{
	int n;
	capu_config cfg = parse("", &n);
	CHECK(n == 0 && cfg.mount == 1);
}

static void test_branch_checks(void)
{
	/* BEQ/BNE/... +0x1A with imm8 0x0D: the expected original instruction. */
	for (unsigned cond = 0; cond <= 0xD; cond++) {
		unsigned char insn[2] = { 0x0D, (unsigned char)(0xD0 | cond) };
		CHECK(capu_is_thumb_cond_branch(insn, 0x0D));
		CHECK(!capu_is_thumb_uncond_branch(insn, 0x0D));
	}
	/* cond 0xE is UDF and 0xF is SVC: not branches. */
	unsigned char udf[2] = { 0x0D, 0xDE }, svc[2] = { 0x0D, 0xDF };
	CHECK(!capu_is_thumb_cond_branch(udf, 0x0D));
	CHECK(!capu_is_thumb_cond_branch(svc, 0x0D));
	/* Different branch distance: wrong place. */
	unsigned char other_imm[2] = { 0x0C, 0xD0 };
	CHECK(!capu_is_thumb_cond_branch(other_imm, 0x0D));
	/* The patched form: B +0x1A. */
	unsigned char patched[2] = { 0x0D, 0xE0 };
	CHECK(capu_is_thumb_uncond_branch(patched, 0x0D));
	CHECK(!capu_is_thumb_cond_branch(patched, 0x0D));
	/* Something unrelated, e.g. "push {r4, lr}" (10 B5). */
	unsigned char push[2] = { 0x10, 0xB5 };
	CHECK(!capu_is_thumb_cond_branch(push, 0x0D) && !capu_is_thumb_uncond_branch(push, 0x0D));
}

int main(void)
{
	test_defaults();
	test_basic();
	test_spacing_case_comments_crlf();
	test_bad_lines_ignored();
	test_last_value_wins_and_no_trailing_newline();
	test_prefix_keys_rejected();
	test_empty();
	test_branch_checks();
	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
