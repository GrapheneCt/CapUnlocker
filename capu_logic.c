/*
 * Copyright (c) 2020 Graphene
 */

#include "capu_logic.h"

void capu_config_defaults(capu_config *cfg)
{
	cfg->mount = 1;
	cfg->core = 1;
	cfg->vm = 1;
	cfg->thread_attr = 1;
	cfg->log = 1;
}

static char lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int is_space(char c)
{
	return c == ' ' || c == '\t' || c == '\r';
}

/* Case-insensitive compare of buf[0..len) with a NUL-terminated word. */
static int word_eq(const char *buf, size_t len, const char *word)
{
	size_t i = 0;
	for (; i < len && word[i]; i++) {
		if (lower(buf[i]) != word[i])
			return 0;
	}
	return i == len && word[i] == '\0';
}

static int parse_bool(const char *buf, size_t len, int *out)
{
	if (word_eq(buf, len, "1") || word_eq(buf, len, "on") ||
	    word_eq(buf, len, "true") || word_eq(buf, len, "yes")) {
		*out = 1;
		return 1;
	}
	if (word_eq(buf, len, "0") || word_eq(buf, len, "off") ||
	    word_eq(buf, len, "false") || word_eq(buf, len, "no")) {
		*out = 0;
		return 1;
	}
	return 0;
}

static int *find_key(capu_config *cfg, const char *key, size_t len)
{
	if (word_eq(key, len, "mount"))
		return &cfg->mount;
	if (word_eq(key, len, "core"))
		return &cfg->core;
	if (word_eq(key, len, "vm"))
		return &cfg->vm;
	if (word_eq(key, len, "thread_attr"))
		return &cfg->thread_attr;
	if (word_eq(key, len, "log"))
		return &cfg->log;
	return 0;
}

int capu_config_parse(capu_config *cfg, const char *buf, size_t len)
{
	int applied = 0;
	size_t pos = 0;

	while (pos < len) {
		size_t line_end = pos;
		while (line_end < len && buf[line_end] != '\n')
			line_end++;

		/* Drop the comment part of the line. */
		size_t end = pos;
		while (end < line_end && buf[end] != '#')
			end++;

		size_t eq = pos;
		while (eq < end && buf[eq] != '=')
			eq++;

		if (eq < end) {
			size_t ks = pos, ke = eq, vs = eq + 1, ve = end;
			while (ks < ke && is_space(buf[ks]))
				ks++;
			while (ke > ks && is_space(buf[ke - 1]))
				ke--;
			while (vs < ve && is_space(buf[vs]))
				vs++;
			while (ve > vs && is_space(buf[ve - 1]))
				ve--;

			int *slot = find_key(cfg, buf + ks, ke - ks);
			int value;
			if (slot && parse_bool(buf + vs, ve - vs, &value)) {
				*slot = value;
				applied++;
			}
		}

		pos = line_end + 1;
	}

	return applied;
}

int capu_is_thumb_cond_branch(const unsigned char *insn, unsigned char imm8)
{
	/* cond 0xE is UDF and 0xF is SVC, not branches. */
	return insn[0] == imm8 && (insn[1] & 0xF0) == 0xD0 && (insn[1] & 0x0F) <= 0x0D;
}

int capu_is_thumb_uncond_branch(const unsigned char *insn, unsigned char imm8)
{
	return insn[0] == imm8 && insn[1] == 0xE0;
}
