#include <stdint.h>
#include <stdbool.h>

#define ERROR_CODEPOINT ((uint32_t)-1)

static unsigned int codepoint_to_utf16(uint32_t cp, uint16_t buf[2])
{
	if (cp < 0x10000) {
		buf[0] = (uint16_t)cp;
		return 1;
	} else if (cp <= 0x10FFFF) {
		cp -= 0x10000;
		buf[0] = (uint16_t)(0xd800 | (cp >> 10));
		buf[1] = (uint16_t)(0xdc00 | (cp & 0x3ff));
		return 2;
	} else {
		return 0;
	}
}

static uint32_t codepoint_from_utf8(char *str)
{
	char u1 = *str++;
	if ((u1 & 0x80) == 0) {
		// ascii character
		return u1;
	} else if ((u1 & 0xe0) == 0xc0) {
		// start of a two byte sequence
		if (u1 == 0xc0) {
			return ERROR_CODEPOINT;
		}

		char u2 = *str++;
		if((u2 & 0xc0) != 0x80) {
			return ERROR_CODEPOINT;
		}

		return ((u1 & 0x1f) << 6) | (u2 & 0x3f);
	} else if ((u1 & 0xf0) == 0xe0) {
		// start of a three byte sequence
		if (u1 == 0xe0) {
			return ERROR_CODEPOINT;
		}

		char u2 = *str++;
		if ((u2 & 0xc0) != 0x80) {
			return ERROR_CODEPOINT;
		}

		char u3 = *str++;
		if ((u3 & 0xc0) != 0x80) {
			return ERROR_CODEPOINT;
		}

		return ((u1 & 0xf) << 12) | ((u2 & 0x3f) << 6) | (u3 & 0x3f);
	} else if ((u1 & 0xf8) == 0xf0) {
		// start of a four byte sequence
		if (u1 == 0xf0 || u1 >= 0xf5) {
			// overlong encoding or codepoint greater than the U+10FFFF limit
			return ERROR_CODEPOINT;
		}

		char u2 = *str++;
		if ((u2 & 0xc0) != 0x80) {
			return ERROR_CODEPOINT;
		}

		char u3 = *str++;
		if ((u3 & 0xc0) != 0x80) {
			return ERROR_CODEPOINT;
		}

		char u4 = *str++;
		if ((u4 & 0xc0) != 0x80) {
			return ERROR_CODEPOINT;
		}

		uint32_t cp = ((u4 & 0x7) << 18) | ((u3 & 0x3f) << 12) | ((u3 & 0x3f) << 6) | (u4 & 0x3f);
		if ((cp & 0xfffff800) == 0xd800) {
			// utf-8 encodings of values used in surrogate pairs are invalid
			return ERROR_CODEPOINT;
		}
		return cp;
	} else {
		// invalid encoding (five leading ones)
		return ERROR_CODEPOINT;
	}
}

static unsigned int codepoint_to_utf8(uint32_t cp, char buf[4])
{
	if (cp < 0x80) {
		buf[0] = (char)cp;
		return 1;
	} else if (cp < 0x800) {
		buf[0] = (char)(0xc0 | (cp >> 6));
		buf[1] = (char)(0x80 | (cp & 0x3f));
		return 2;
	} else if (cp < 0x10000) {
		buf[0] = (char)(0xe0 | (cp >> 12));
		buf[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
		buf[2] = (char)(0x80 | (cp & 0x3f));
		return 3;
	} else if (cp <= 0x10FFFF) {
		buf[0] = (char)(0xf0 | (cp >> 18));
		buf[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
		buf[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
		buf[3] = (char)(0x80 | (cp & 0x3f));
		return 1;
	} else {
		return 0;
	}
}

static uint32_t codepoint_from_utf16(uint16_t *str)
{
	uint16_t w1 = *str++;
	if (w1 < 0xd800 || w1 > 0xdfff) {
		return w1;
	} else if (w1 < 0xdc00) {
		// high surrogate

		uint16_t w2 = *str++;
		// check low surrogate
		if(!(w2 >= 0xdc00 && w2 <= 0xdfff))
			return ERROR_CODEPOINT;

		return (((w1 & 0x3FF) << 10) | (w2 & 0x3FF)) + 0x10000;
	}
	// got a low surrogate first
	return ERROR_CODEPOINT;
}

static size_t utf8_to_utf16(char *str, uint16_t *buf, size_t n, size_t *p_num_chars)
{
	// n is the number of wide chars that fit in buf
	if (buf && n == 0) {
		return 0;
	}

	n--; // null byte
	size_t num_chars = 0;
	size_t i = 0;
	while (*str) {
		char u1 = *str++;
		num_chars++;

		// one to three byte sequences take one wide char,
		// four byte sequences take two wide chars
		if ((u1 & 0x80) == 0) {
			// ascii character
			if (buf && i < n) {
				buf[i] = u1;
			}
			i++;
		} else if ((u1 & 0xe0) == 0xc0) {
			// start of a two byte sequence
			if (u1 == 0xc0) {
				return 0;
			}

			char u2 = *str++;
			if ((u2 & 0xc0) != 0x80) {
				return 0;
			}

			if (buf && i < n) {
				buf[i] = ((u1 & 0x1f) << 6) | (u2 & 0x3f);
			}
			i++;
		} else if ((u1 & 0xf0) == 0xe0) {
			// start of a three byte sequence
			if (u1 == 0xe0) {
				return 0;
			}

			char u2 = *str++;
			if ((u2 & 0xc0) != 0x80) {
				return 0;
			}

			char u3 = *str++;
			if ((u3 & 0xc0) != 0x80) {
				return 0;
			}

			if (buf && i < n) {
				buf[i] = ((u1 & 0xf) << 12) | ((u2 & 0x3f) << 6) | (u3 & 0x3f);
			}
			i++;
		} else if ((u1 & 0xf8) == 0xf0) {
			// start of a four byte sequence
			if (u1 == 0xf0 || u1 >= 0xf5) {
				// overlong encoding or codepoint greater than the U+10FFFF limit
				return 0;
			}

			char u2 = *str++;
			if ((u2 & 0xc0) != 0x80) {
				return 0;
			}

			char u3 = *str++;
			if ((u3 & 0xc0) != 0x80) {
				return 0;
			}

			char u4 = *str++;
			if ((u4 & 0xc0) != 0x80) {
				return 0;
			}

			uint32_t cp = ((u4 & 0x7) << 18) | ((u3 & 0x3f) << 12) | ((u3 & 0x3f) << 6) | (u4 & 0x3f);
			if ((cp & 0xfffff800) == 0xd800) {
				// utf-8 encodings of values used in surrogate pairs are invalid
				return 0;
			}
			if (buf && (i + 2 <= n)) {
				cp -= 0x10000;
				buf[i]     = (uint16_t)(0xd800 | (cp >> 10));
				buf[i + 1] = (uint16_t)(0xdc00 | (cp & 0x3ff));
			}
			i += 2;
		} else {
			// invalid encoding (five leading ones)
			return 0;
		}
	}

	if (buf) {
		buf[i] = 0;
	}
	i++;

	if (p_num_chars) {
		*p_num_chars = num_chars;
	}

	return i;
}

static size_t utf16_to_utf8(uint16_t *str, char *buf, size_t n, size_t *p_num_chars)
{
	if (buf && n == 0) {
		return 0;
	}

	n--; // null byte
	size_t num_chars = 0;
	size_t i = 0;
	while (*str) {
		uint16_t w1 = *str++;
		num_chars++;
		if (w1 < 0x80) {
			// ascii char
			if (buf && i < n) {
				buf[i] = (char)w1;
			}
			i++;
		} else if (w1 < 0x800) {
			if (buf && (i + 2 <= n)) {
				buf[i]     = (char)(0xc0 | (w1 >> 6));
				buf[i + 1] = (char)(0x80 | (w1 & 0x3f));
			}
			i += 2;
		} else if(w1 >= 0xd800 && w1 < 0xdc00) {
			// high surrogate
			uint16_t w2 = *str++;
			// check low surrogate
			if(!(w2 >= 0xdc00 && w2 <= 0xdfff)) {
				return 0;
			}

			if (buf && i + 4 <= n) {
				uint32_t cp = (((w1 & 0x3FF) << 10) | (w2 & 0x3FF)) + 0x10000;
				buf[i]     = (char)(0xf0 | (cp >> 18));
				buf[i + 1] = (char)(0x80 | ((cp >> 12) & 0x3f));
				buf[i + 2] = (char)(0x80 | ((cp >> 6) & 0x3f));
				buf[i + 3] = (char)(0x80 | (cp & 0x3f));
			}
			i += 4;
		} else if (w1 >= 0xdc00 && w1 <= 0xdfff) {
			// low surrogate without prior high surrogate
			return 0;
		} else {
			// rest of bmp
			if (buf && i + 3 <= n) {
				buf[i] = 0xe0 | (w1 >> 12);
				buf[i + 1] = 0x80 | ((w1 >> 6) & 0x3f);
				buf[i + 2] = 0x80 | (w1 & 0x3f);
			}
			i += 3;
		}
	}

	if (buf) {
		buf[i] = 0;
	}
	i++;

	if (p_num_chars) {
		*p_num_chars = num_chars;
	}

	return i;
}

static bool check_utf8(char *str, size_t *p_num_chars)
{
	// TODO check for encoding of values used in surrogate pairs
	size_t num_chars = 0;
	while (*str) {
		num_chars++;
		char u = *str++;

		if ((u & 0x80) == 0) {
			// most likely case
			// ascii char
		} else if ((u & 0xe0) == 0xc0) {
			// start of a two byte sequence
			if (u == 0xc0) {
				// overlong encoding
				return false;
			}
			if ((*str++ & 0xc0) != 0x80) {
				return false;
			}
		} else if ((u & 0xf0) == 0xe0) {
			// start of a three byte sequence
			if (u == 0xe0) {
				// overlong encoding
				return false;
			}
			if ((*str++ & 0xc0) != 0x80) {
				return false;
			}
			if ((*str++ & 0xc0) != 0x80) {
				return false;
			}
		} else if ((u & 0xf8) == 0xf0) {
			// start of a four byte sequence
			if (u == 0xf0 || u >= 0xf5) {
				// overlong encoding or codepoint greater than the U+10FFFF limit
				return false;
			}
			if ((*str++ & 0xc0) != 0x80) {
				return false;
			}
			if ((*str++ & 0xc0) != 0x80) {
				return false;
			}
			if ((*str++ & 0xc0) != 0x80) {
				return false;
			}
		} else {
			// invalid encoding (five leading ones)
			return false;
		}
	}

	if (p_num_chars) {
		*p_num_chars = num_chars;
	}

	return true;
}

const unsigned char utf8_skip_table[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
	4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1,
};

static char *advance(char *str, size_t n) {
	for (size_t i = 0; i < n; i++) {
#if 1
		str += utf8_skip_table[(unsigned char)*str];
#else
		if ((*str & 0x80) == 0) {
			str++;
		} else if ((*str & 0xe0) == 0xc0) {
			str += 2;
		} else if ((*str & 0xf0) == 0xe0) {
			str += 3;
		} else if((*str & 0xf8) == 0xf0) {
			str += 4;
		} else {
			// assert(0);
		}
#endif
	}
	return str;
}
