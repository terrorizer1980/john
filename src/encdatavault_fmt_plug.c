/*
 * Cracker for ENCSecurity Data Vault.
 *
 * This software is Copyright (c) 2021 Sylvain Pelissier <sylvain.pelissier at kudelskisecurity.com>
 * and it is hereby released to the general public under the following terms:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_encdadatavault;
#elif FMT_REGISTERS_H
john_register_one(&fmt_encdadatavault);
#else

#include <string.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "aes.h"
#include "md5.h"
#include "common.h"
#include "formats.h"

#define FORMAT_TAG              "$encdv$"
#define FORMAT_TAG_LENGTH       (sizeof(FORMAT_TAG) - 1)
#define FORMAT_LABEL            "ENCDataVault"
#define FORMAT_NAME             ""
#define ALGORITHM_NAME          "MD5"
#define BENCHMARK_COMMENT       " (1000 iterations)"
#define BENCHMARK_LENGTH        0x107
#define PLAINTEXT_LENGTH        125
#define BINARY_SIZE             0
#define BINARY_ALIGN            1
#define SALT_SIZE               sizeof(struct custom_salt)
#define SALT_ALIGN              4
#define MIN_KEYS_PER_CRYPT      1
#define MAX_KEYS_PER_CRYPT      4

#ifndef OMP_SCALE
#define OMP_SCALE               16
#endif

#define ENC_DEFAULT_NUM_HASH_ITERATIONS 1000
#define ENC_SALT_SIZE 16
#define ENC_IV_SIZE 16
#define ENC_BLOCK_SIZE 16
#define ENC_KEY_SIZE 16
#define ENC_NONCE_SIZE 8
#define ENC_SIG_SIZE 4
#define ENC_MAX_KEY_NUM 8
#define ENC_KEYCHAIN_SIZE 128

typedef union buffer_128_u {
	uint8_t u8[8];
	uint64_t u64[2];
} buffer_128;

static MAYBE_INLINE void enc_xor_block(uint64_t *dst, const uint64_t *src)
{
	dst[0] ^= src[0];
	dst[1] ^= src[1];
}

// md5("f46fcf4f0d9d45b198eb240ff819aac0\x00\x00\x00" + index)
static const char *const default_salts[] = {
	"\x0f\xc9\xe7\xd0\x8b\xe4\x24\xf6\x56\x9d\x4e\x72\xed\xbc\x2c\x5c",
	"\xdd\x79\x74\xf3\x3d\x83\x00\xc2\x9b\xd2\x93\xd5\x7f\x9d\x9b\x8c",
	"\x60\x85\x0c\x47\x58\x46\xe2\x96\x2d\x99\x5d\x5e\xf1\xd0\x6a\x28",
	"\xe2\x3f\x3d\x6b\x99\x61\x4b\xa9\xc4\xed\xc5\xdd\xd8\x25\x3c\xe1",
	"\x2c\xa4\x59\x89\x1d\x78\x52\xdb\x30\x31\xd0\x9f\x9f\x34\x88\x35",
	"\xdb\x1b\xb5\x27\xe8\x21\x4f\x79\xa0\xb2\xcb\x32\x42\xd9\xf2\x0a",
	"\xae\xa8\xb6\x8e\xd0\x7b\x62\xa1\x40\x0e\x17\xc6\xad\x64\x20\xc8",
	"\xea\xe3\xf4\x4e\xaf\x4a\x8f\x84\xf1\xfa\xb3\x08\x85\x69\xbe\xf8"
};

static struct fmt_tests encdatavault_tests[] = {
	// Sandisk vaults
	{ "$encdv$1$1$ae07a8354f6fe3ca$a6066363", "bbbb" },
	{ "$encdv$1$1$7531d98593ea7b1d$d070650d", "openwall" },
	// Sony vault
	{ "$encdv$1$2$e8a5d78fa5511fa2$b96a4747", "bbbb" },
	{ "$encdv$1$2$4e277e5547b88f9c$eb0ed650", "openwallopenwall!" },
	// ENCDataVault 512 bits user password
	{ "$encdv$1$3$d6209d17c0a87818$77608f04", "123456789ABCDEf" },
	{ "$encdv$1$3$3563390c4d66944d$2b0470f5", "openwallopenwall" },
	// ENCDataVault 128 bits vault
	{ "$encdv$3$1$75c97f784cad5027$c58e34a9$6fa3c4085acadda7c94589fe3dd8209d59a79b0ea659c4af51861f659c2d6cc645dab7c821c2cc0e8da97ce66e9be779fcf8fc33c1250aee2cd46e08a3864763a5c7f790c9965376a36fbf3b1c8b944d096e3bbe586a952f9fab5ee2f1c1ca7d2cd06ff357bb397c3eb1da66c5562998411c0b2dff04860e6f6adf818c853941", "128vaultTest" },
	{ "$encdv$3$1$af9479e81a896dbb$e643f5f6$8e8fd6ccf10cf651efcb561a167fa301849bef0a28718f67d31d53a1e7a8328b3aaa6c642d8a2483e8de5c3751fd0b81604ead7c6412e6197886735bcbd1dd50d23f293bf11856986bd07a7513275dea991968376703a643fc19c373c1d62a7779db88068e24752bb6761f42c8aec9cf7a80ef2b7f6384212ee6b670d8ba17a7", "OpenwallOpenwall" },
	// ENCDataVault 256 bits vault
	{ "$encdv$3$2$a44a9a62e131023e$d9fe5a04$0bc8c0c937cac8de5a226a9dc7ff5d2542bb8814973afdcc0e593fded8b337cac18acd60c1afea3550fdeabee339a36892eb99f0f502b9f74075e9cb26970e983189be1395ffbec8ebfb765a563db45c3e53d73040e41bcfe58dc211f03f1384c5080c298a5e4bdf7cf23b893b3d2dbe05c472180fdc3f7fd82d97ea0eec1e7e", "OpenwallOpenwall" },
	{ "$encdv$3$2$85089cdde5cb099b$c3c98127$d96fd1e2709908e08994d1809e516985cf028cd73e2a829dee1ce7473c3c4d0ce35a2652b58ed7ad046641259b9a24a046c89d3251e1036e7117dd8221b07f5a45b6c215b3ad308e5f7b248ea8f1fabaa23da4840797c9052c3f8578187d514a92356b8138455db6424a41db6b0de2817d463f23d44f39537a6c28b2ba075a01", "Verylongbutstillfailpassword"},
	// ENCDataVault 512 bits vault
	{ "$encdv$3$3$bd837ac896f26983$15107b99$fb141e6db2cb586b2b445d6a6ca47f17d16947afa32298cc30c6fcdc4bfac7a874d53f2de4bcb196645e59e1c8e5883999875c9951d637e08f78d2bb16003c2abe8bfa1cb8b8d7b627828d61d775937308e7e119dea727da8af12490d50e8b8dc8f24daa7e101576112b52374f3ea4a73f7fa6bfb802bd6dfa845318a2884a9e", "123456789ABCDEf" },
	{ "$encdv$3$3$ffd2a778b7f1304b$311893a8$5e48500065fc4138b134ce7858ac3b29d9f4b19c5f0d7ee1d07d8dd4dc3d5ae56b18e84d822087849573074a5776dee5309e5cb6bbd0d1470e0717463119bb080c96e24ebd563673060397803aebae5df7c59defcc8fa687b96c9a8245e540be699061c299a69830472e1a6a74ad72086dfac906a49e0ce84fc722da9715d675", "OpenwallOpenwall" },
	// ENCDataVault 1024 bits vault
	{ "$encdv$3$4$bc93a92cf625e360$fe120bfb$739f9d85964cef2f63b927ff77f3328cc5192014ae21c29954c322f4e808fe5c8abe64cc150dbcb08cb334f3b5c28357f10d8d5c6a103e2c899402136c14633aaf8a5347959b33b80ade2e3f5698864605940dc1423704999e5da859d6584491bbe940f00f162c75d2766b868ef2b4c6bf599a2e8ccea3f7cdfab193744a8d10", "123456789ABCDEf" },
	{ "$encdv$3$4$e9786d82101f30ef$d690010a$5748af61f246c1569cc72cbd6c97e0be46b1612842fd3c6284637af5351508a602c731739acb056b845dfa2b5befd40f14136b4336e0c8e98144555a90befbc85abfc4069fbb71709fa39e29f8c6a98d28e14251e50ffdd43d75252de2b0b14c386e72927c62ae39ba18ff8b32a339d882fdd0b15284af246fa50b7c6992f783", "Verylongbutstillfailpassword" },
	{ NULL }
};

static int *cracked;
static int any_cracked;
static size_t cracked_size;

static char (*saved_key)[PLAINTEXT_LENGTH + 1];

static struct custom_salt {
	unsigned int version;
	unsigned int algo_id;
	unsigned char salt[ENC_SALT_SIZE];
	unsigned char iv[ENC_IV_SIZE];
	unsigned char encrypted_data[ENC_BLOCK_SIZE];
	unsigned char keychain[ENC_KEYCHAIN_SIZE];
} *cur_salt;

void static enc_aes_ctr_iterated(const unsigned char *in, unsigned char *out, const unsigned char *key,
                                 buffer_128 ivs[ENC_MAX_KEY_NUM], size_t len, size_t nb_keys, uint64_t counter)
{
	AES_KEY aes_key;
	buffer_128 tmp_iv;
	buffer_128 tmp_out;
	int i, j;

	AES_set_encrypt_key(key, ENC_KEY_SIZE * 8, &aes_key);
	len >>= 4;
	counter <<= 56;

	for (i = 0; i < len; i++) {
		tmp_iv.u64[0] = ivs[0].u64[0];
		tmp_iv.u64[1] = counter;
		AES_encrypt(tmp_iv.u8, tmp_iv.u8, &aes_key);
		memcpy(tmp_out.u8, in, AES_BLOCK_SIZE);
		enc_xor_block(tmp_out.u64, tmp_iv.u64);

		for (j = 1; j < nb_keys; j++) {
			tmp_iv.u64[0] = ivs[j].u64[0];
			tmp_iv.u64[1] = counter;
			AES_encrypt(tmp_iv.u8, tmp_iv.u8, &aes_key);
			enc_xor_block(tmp_out.u64, tmp_iv.u64);
		}
		memcpy(out, tmp_out.u8, AES_BLOCK_SIZE);

		// Increment counter, only 255 block encryption is supported.
		counter += 0x0100000000000000;
		out += AES_BLOCK_SIZE;
		in += AES_BLOCK_SIZE;
	}
}

static void init(struct fmt_main *self)
{
	omp_autotune(self, OMP_SCALE);
	saved_key = mem_calloc(sizeof(*saved_key), self->params.max_keys_per_crypt);
	cracked = mem_calloc(sizeof(*cracked), self->params.max_keys_per_crypt);
	cracked_size = sizeof(*cracked) * self->params.max_keys_per_crypt;
	any_cracked = 0;
}

static void done(void)
{
	MEM_FREE(saved_key);
	MEM_FREE(cracked);
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *p = ciphertext, *ctcopy, *keeptr;
	int extra;

	if (strncmp(ciphertext, FORMAT_TAG, FORMAT_TAG_LENGTH) != 0)
		return 0;

	ctcopy = strdup(ciphertext);
	keeptr = ctcopy;
	ctcopy += FORMAT_TAG_LENGTH;
	if ((p = strtokm(ctcopy, "$")) == NULL) // version
		goto err;
	if (strcmp(p, "1") && strcmp(p, "3")) {
		static int warned;
		if (!self_test_running && !warned++)
			fprintf(stderr, "%s: Warning: version %d not supported, not loading such hashes!\n", self->params.label, atoi(p));
		goto err;
	}
	char version = *p;
	if ((p = strtokm(NULL, "$")) == NULL)   // algorithm id
		goto err;
	if (!isdec(p))
		goto err;
	if (atoi(p) > 4 || atoi(p) < 1) {
		static int warned;
		if (!warned++)
			fprintf(stderr, "%s: Warning: algorithm id %d not supported!\n", self->params.label, atoi(p));
		goto err;
	}
	if ((p = strtokm(NULL, "$")) == NULL)   // Nonce
		goto err;
	if (hexlenl(p, &extra) != ENC_NONCE_SIZE * 2 || extra)
		goto err;
	if ((p = strtokm(NULL, "$")) == NULL)   // Encrypted header
		goto err;
	if (hexlenl(p, &extra) != ENC_SIG_SIZE * 2 || extra)
		goto err;
	if (version == '3') {
		if ((p = strtokm(NULL, "$")) == NULL)	// Keychain
				goto err;
		if (hexlenl(p, &extra) != ENC_KEYCHAIN_SIZE * 2 || extra)
				goto err;
    }
	MEM_FREE(keeptr);
	return 1;

err:
	MEM_FREE(keeptr);
	return 0;
}

static void *get_salt(char *ciphertext)
{
	static struct custom_salt cs;
	int i;
	char *p = ciphertext, *ctcopy, *keeptr;

	memset(&cs, 0, sizeof(cs));
	ctcopy = strdup(ciphertext);
	keeptr = ctcopy;
	ctcopy += FORMAT_TAG_LENGTH;
	p = strtokm(ctcopy, "$");
	cs.version = atoi(p);
	p = strtokm(NULL, "$");
	cs.algo_id = atoi(p);
	p = strtokm(NULL, "$");
	for (i = 0; i < ENC_NONCE_SIZE; i++)
		cs.iv[i] = (atoi16[ARCH_INDEX(p[2 * i])] << 4) | atoi16[ARCH_INDEX(p[2 * i + 1])];

	// Init AES CTR counter
	cs.iv[ENC_IV_SIZE - 1] = 1;

	p = strtokm(NULL, "$");
	for (i = 0; i < ENC_SIG_SIZE; i++)
		cs.encrypted_data[i + 4] = (atoi16[ARCH_INDEX(p[2 * i])] << 4) | atoi16[ARCH_INDEX(p[2 * i + 1])];

	// Keychain for version 3
	if (cs.version == 3) {
		p = strtokm(NULL, "$");
		for (i = 0; i < ENC_KEYCHAIN_SIZE; i++)
			cs.keychain[i] = (atoi16[ARCH_INDEX(p[2 * i])] << 4) | atoi16[ARCH_INDEX(p[2 * i + 1])];
	}

	MEM_FREE(keeptr);
	return (void *)&cs;
}

static void set_salt(void *salt)
{
	cur_salt = (struct custom_salt *)salt;
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index;
	int nb_keys;

	if (any_cracked) {
		memset(cracked, 0, cracked_size);
		any_cracked = 0;
	}

	nb_keys = 1 << (cur_salt->algo_id - 1);

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (index = 0; index < count; index++) {
		int j;
		MD5_CTX ctx;
		buffer_128 kdf_out[ENC_MAX_KEY_NUM];
		buffer_128 hash;
		buffer_128 tmp;
		buffer_128 ivs[ENC_MAX_KEY_NUM];
		unsigned char result[ENC_KEY_SIZE * ENC_MAX_KEY_NUM] = { 0 };

		// Key derivation
		tmp.u64[0] = 0;
		tmp.u64[1] = 0;
		MD5_Init(&ctx);
		MD5_Update(&ctx, saved_key[index], strlen(saved_key[index]));
		MD5_Final(hash.u8, &ctx);

		for (j = 0; j < ENC_MAX_KEY_NUM; j++) {
			memcpy(kdf_out[j].u8, default_salts[j], ENC_SALT_SIZE);
		}

		for (j = 1; j < ENC_DEFAULT_NUM_HASH_ITERATIONS; j++) {
			MD5_Init(&ctx);
			MD5_Update(&ctx, hash.u8, 16);
			MD5_Final(hash.u8, &ctx);
			enc_xor_block(tmp.u64, hash.u64);
		}

		for (j = 0; j < ENC_MAX_KEY_NUM; j++) {
			enc_xor_block(kdf_out[j].u64, tmp.u64);
		}

		/* AES iterated CTR */
		if (cur_salt->version == 1) {
			memcpy(ivs[0].u8, cur_salt->iv, ENC_NONCE_SIZE);
			for (j = 1; j < nb_keys; j++) {
				memcpy(ivs[j].u8, cur_salt->iv, ENC_NONCE_SIZE);
				ivs[j].u64[0] ^= kdf_out[j].u64[0];
			}
			// result buffer is used here to hold the decrypted data.
			enc_aes_ctr_iterated(cur_salt->encrypted_data, result, kdf_out[0].u8, ivs, AES_BLOCK_SIZE, nb_keys, 1);
			if (!memcmp(result + 4, "\xd2\xc3\xb4\xa1", ENC_SIG_SIZE)) {
				cracked[index] = 1;
#ifdef _OPENMP
#pragma omp atomic
#endif
				any_cracked |= 1;
			}
		} else {
			// Decrypt keychain
			ivs[0].u64[0] = 0;
			for (j = 1; j < ENC_MAX_KEY_NUM; j++) {
				ivs[j].u64[0] = kdf_out[ENC_MAX_KEY_NUM - j].u64[0];
			}
			// result buffer is used for the decrypted keys from the keychain
			enc_aes_ctr_iterated(cur_salt->keychain, result, kdf_out[0].u8, ivs, ENC_KEYCHAIN_SIZE, ENC_MAX_KEY_NUM, 0);

			// Decrypt data
			memcpy(ivs[0].u8, cur_salt->iv, ENC_NONCE_SIZE);
			for (j = 1; j < nb_keys; j++) {
				memcpy(ivs[j].u8, cur_salt->iv, ENC_NONCE_SIZE);
				memcpy(tmp.u8, result + j * 16, ENC_NONCE_SIZE);
				ivs[j].u64[0] ^= tmp.u64[0];
			}
			// result buffer is reused here to hold the decrypted data.
			enc_aes_ctr_iterated(cur_salt->encrypted_data, result, result, ivs, AES_BLOCK_SIZE, nb_keys, 1);
			if (!memcmp(result + 4, "\xd2\xc3\xb4\xa1", ENC_SIG_SIZE)) {
				cracked[index] = 1;
#ifdef _OPENMP
#pragma omp atomic
#endif
				any_cracked |= 1;
			}
		}
	}
	return count;
}

static int cmp_all(void *binary, int count)
{
	return any_cracked;
}

static int cmp_one(void *binary, int index)
{
	return cracked[index];
}

static int cmp_exact(char *source, int index)
{
	return cracked[index];
}

static void set_key(char *key, int index)
{
	strnzcpy(saved_key[index], key, sizeof(saved_key[index]));
}

static char *get_key(int index)
{
	return saved_key[index];
}

struct fmt_main fmt_encdadatavault = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		0,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_8_BIT | FMT_OMP | FMT_NOT_EXACT,
		{ NULL},
		{ FORMAT_TAG},
		encdatavault_tests
	}, {
		init,
		done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		fmt_default_binary,
		get_salt,
		{ NULL},
		fmt_default_source,
		{
			fmt_default_binary_hash
		},
		fmt_default_salt_hash,
		NULL,
		set_salt,
		set_key,
		get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
			fmt_default_get_hash
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};

#endif                          /* plugin stanza */
