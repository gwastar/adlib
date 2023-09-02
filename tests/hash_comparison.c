#include <assert.h>
#include <gnutls/gnutls.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_XXHASH
#include <xxhash.h>
#endif
#ifdef HAVE_GCRYPT
#include <gcrypt.h>
#endif
#ifdef HAVE_GNUTLS
#include <gnutls/crypto.h>
#endif
#include "array.h"
#include "compiler.h"
#include "hash.h"
#include "hashtable.h"
#include "random.h"

array_t(char *) words = NULL;
char **uuids;
char **numbers;

DEFINE_HASHTABLE(stringset, const char *, const char *, 8, strcmp(*key, *entry) == 0)

	static int compare_doubles(const void *_a, const void *_b)
{
	double a = *(const double *)_a;
	double b = *(const double *)_b;
	return a < b ? -1 : (a == b ? 0 : 1);
}

static double get_median(double *values, size_t n)
{
	qsort(values, n, sizeof(values[0]), compare_doubles);
	double x = 0.5 * (n - 1);
	size_t idx0 = (size_t)x;
	size_t idx1 = idx0 + (n != 1);
	double fract = x - idx0;
	return (1 - fract) * values[idx0] + fract * values[idx1];
}

static double ns_elapsed(struct timespec start, struct timespec end)
{
	double s = end.tv_sec - start.tv_sec;
	double ns = end.tv_nsec - start.tv_nsec;
	return (1.0 / 1000000000) * ns + s;
}

static double benchmark_single(const char **strings, size_t num_strings, uint32_t (*hash)(const char *))
{
	struct timespec start_tp, end_tp;
	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_tp);

	struct stringset set;
	stringset_init(&set, 128);

	for (size_t i = 0; i < num_strings; i++) {
		*stringset_insert(&set, strings[i], hash(strings[i])) = strings[i];
	}

	for (size_t i = 0; i < num_strings; i++) {
		assert(stringset_lookup(&set, strings[i], hash(strings[i])));
	}

	for (size_t i = 0; i < num_strings; i++) {
		assert(stringset_remove(&set, strings[i], hash(strings[i]), NULL));
	}

	stringset_destroy(&set);

	clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_tp);
	return ns_elapsed(start_tp, end_tp);
}

static double benchmark(const char **strings, size_t num_strings, uint32_t (*hash)(const char *))
{
	double times[5];
	size_t n = sizeof(times) / sizeof(times[0]);
	for (size_t i = 0; i < n; i++) {
		times[i] = benchmark_single(strings, num_strings, hash);
	}
	return get_median(times, n);
}

static uint32_t murmur3(const char *str)
{
	return murmurhash3_x64_64(str, strlen(str), 0).u64;
}

static const uint64_t siphash_key[] = {0xdeadbeef, 0xcafebabe};

static uint32_t siphash24(const char *str)
{
	return siphash24_64(str, strlen(str), siphash_key).u64;
}

static uint32_t siphash13(const char *str)
{
	return siphash13_64(str, strlen(str), siphash_key).u64;
}

static uint32_t fnv1(const char *str)
{
	uint32_t hash = 2166136261;
	uint8_t c;
	while ((c = *str++)) {
		hash *= 16777619;
		hash ^= c;
	}
	return hash;
}

static uint32_t fnv1a(const char *str)
{
	uint32_t hash = 2166136261;
	uint8_t c;
	while ((c = *str++)) {
		hash ^= c;
		hash *= 16777619;
	}
	return hash;
}

// http://www.cse.yorku.ca/~oz/hash.html
static uint32_t djb2(const char *str)
{
	unsigned long hash = 5381;
	int c;
	while ((c = *str++)) {
		hash =  hash * 33 + c;
	}
	return hash;
}

static uint32_t sdbm(const char *str)
{
	unsigned long hash = 0;
        int c;
        while ((c = *str++)) {
		hash = c + (hash << 6) + (hash << 16) - hash;
	}
        return hash;
}

#ifdef HAVE_XXHASH

static uint32_t xxh32(const char *str)
{
	return XXH32(str, strlen(str), 0);
}

static uint32_t xxh64(const char *str)
{
	return XXH64(str, strlen(str), 0);
}

static uint32_t xxh3_64(const char *str)
{
	return XXH3_64bits(str, strlen(str));
}

#endif

#ifdef HAVE_GNUTLS

static uint32_t gnutls_md5(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gnutls_hash_fast(GNUTLS_DIG_MD5, str, strlen(str), &hash);
	return hash.u32;
}

static uint32_t gnutls_sha1(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gnutls_hash_fast(GNUTLS_DIG_SHA1, str, strlen(str), &hash);
	return hash.u32;
}

static uint32_t gnutls_sha256(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gnutls_hash_fast(GNUTLS_DIG_SHA256, str, strlen(str), &hash);
	return hash.u32;
}

static uint32_t gnutls_sha3(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gnutls_hash_fast(GNUTLS_DIG_SHA3_224, str, strlen(str), &hash);
	return hash.u32;
}

#endif

#ifdef HAVE_GCRYPT

static uint32_t gcrypt_md5(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gcry_md_hash_buffer(GCRY_MD_MD5, &hash, str, strlen(str));
	return hash.u32;
}

static uint32_t gcrypt_sha1(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gcry_md_hash_buffer(GCRY_MD_SHA1, &hash, str, strlen(str));
	return hash.u32;
}

static uint32_t gcrypt_sha256(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gcry_md_hash_buffer(GCRY_MD_SHA256, &hash, str, strlen(str));
	return hash.u32;
}

static uint32_t gcrypt_blake2(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gcry_md_hash_buffer(GCRY_MD_BLAKE2S_128, &hash, str, strlen(str));
	return hash.u32;
}

static uint32_t gcrypt_sha3(const char *str)
{
	union {
		uint32_t u32;
		unsigned char bytes[32];
	} hash;
	gcry_md_hash_buffer(GCRY_MD_SHA3_224, &hash, str, strlen(str));
	return hash.u32;
}

#endif

int main(int argc, char **argv)
{
	if (argc < 2) {
		fputs("pass a text file containing unique lines (words.txt)\n", stderr);
		return 1;
	}
	{
		FILE *file = fopen(argv[1], "r");
		assert(file);
		char buf[128];
		while (fgets(buf, sizeof(buf), file)) {
			assert(buf[strlen(buf) - 1] == '\n');
			buf[strlen(buf) - 1] = '\0';
			array_add(words, strdup(buf));
		}
		assert(!ferror(file));
		fclose(file);
	}
	array_shrink_to_fit(words);
	size_t count = array_length(words);
	printf("%zu words\n", count);

	uuids = malloc(count * sizeof(uuids[0]));
	numbers = malloc(count * sizeof(numbers[0]));
	assert(uuids && numbers);
	struct random_state rng;
	random_state_init(&rng, 0xdeadbeef);
	for (size_t i = 0; i < count; i++) {
		char buf[64];
		sprintf(buf, "%zu", i);
		numbers[i] = strdup(buf);
		sprintf(buf, "%016" PRIx64 "%016" PRIx64, random_next_u64(&rng), random_next_u64(&rng));
		uuids[i] = strdup(buf);
	}

	struct {
		const char *name;
		uint32_t (*hash_func)(const char *);
	} benchmarks[] = {
#define B(name) {#name, name}
		B(murmur3),
		B(siphash24),
		B(siphash13),
		B(fnv1),
		B(fnv1a),
		B(djb2),
		B(sdbm),
#ifdef HAVE_XXHASH
		B(xxh32),
		B(xxh64),
		B(xxh3_64),
#endif
#ifdef HAVE_GNUTLS
		B(gnutls_md5),
		B(gnutls_sha1),
		B(gnutls_sha256),
		B(gnutls_sha3),
#endif
#ifdef HAVE_GCRYPT
		B(gcrypt_md5),
		B(gcrypt_sha1),
		B(gcrypt_sha256),
		B(gcrypt_blake2),
		B(gcrypt_sha3),
#endif
#undef B
	};

	for (size_t i = 0; i < sizeof(benchmarks) / sizeof(benchmarks[0]); i++) {
		const char *name = benchmarks[i].name;
		if (argc > 2 && !strstr(name, argv[1])) {
			continue;
		}
		printf("%s\n", name);
		uint32_t (*hash_func)(const char *) = benchmarks[i].hash_func;
		double t1 = benchmark((const char **)words, count, hash_func);
		printf("\twords:   %.1f ms\n", 1000 * t1);
		double t2 = benchmark((const char **)numbers, count, hash_func);
		printf("\tnumbers: %.1f ms\n", 1000 * t2);
		double t3 = benchmark((const char **)uuids, count, hash_func);
		printf("\tuuids:   %.1f ms\n", 1000 * t3);
		printf("\t----------------\n");
		printf("\tavg:     %.1f ms\n", (1000.0 / 3) * (t1 + t2 + t3));
	}
}
