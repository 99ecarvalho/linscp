/*
 * PuTTY's symmetric crypto against published vectors, every implementation
 * the build carries: software, and AES-NI where the CPU has it.
 *
 * WinSCP compiles part of this code with MSVC (libs/puttyvs) and only ever
 * for 32-bit x86; here clang builds all of it for x86-64. A difference there
 * shows up as wrong output, not as a build error -- AES-NI's counter mode was
 * wrong this way (putty-aes-ni-vector-constants.patch) -- so each primitive
 * is checked directly. Hardware and software AES are also compared across
 * the counter's 64-bit carry, which no single published vector reaches.
 */
#include <stdio.h>
#include <string.h>

#include "putty.h"
#include "ssh.h"

bool aes_ni_available(void);

static int failures = 0;

static void unhex(const char *s, unsigned char *out)
{
    for (size_t i = 0; s[2 * i]; i++)
        sscanf(s + 2 * i, "%2hhx", &out[i]);
}

static void expect(const char *what, const unsigned char *got, const char *want)
{
    unsigned char w[64];
    size_t n = strlen(want) / 2;
    unhex(want, w);
    if (memcmp(got, w, n) != 0) {
        printf("FAIL %s\n  got  ", what);
        for (size_t i = 0; i < n; i++)
            printf("%02x", got[i]);
        printf("\n  want %s\n", want);
        failures++;
    }
}

/* NIST SP 800-38A: F.5.1 / F.5.5 (CTR), F.2.1 (CBC); two blocks each. */
static const char P[] =
    "6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51";
static const char KEY128[] = "2b7e151628aed2a6abf7158809cf4f3c";
static const char KEY256[] =
    "603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4";
static const char CTR_IV[] = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
static const char CBC_IV[] = "000102030405060708090a0b0c0d0e0f";

static void cipher(const char *what, const ssh_cipheralg *alg, const char *key,
                   const char *iv, const char *want, bool decrypt_too)
{
    unsigned char k[32], v[16], buf[32];
    ssh_cipher *c = ssh_cipher_new(alg);
    if (!c) {
        printf("FAIL %s: not available\n", what);
        failures++;
        return;
    }
    unhex(key, k);
    unhex(iv, v);
    unhex(P, buf);
    ssh_cipher_setkey(c, k);
    ssh_cipher_setiv(c, v);
    ssh_cipher_encrypt(c, buf, 32);
    expect(what, buf, want);
    if (decrypt_too) {
        ssh_cipher_setiv(c, v);
        ssh_cipher_decrypt(c, buf, 32);
        expect(what, buf, P);
    }
    ssh_cipher_free(c);
}

static void hash(const char *what, const ssh_hashalg *alg, const char *want)
{
    unsigned char out[64];
    ssh_hash *h = ssh_hash_new(alg);
    put_data(h, "abc", 3);
    ssh_hash_final(h, out);
    expect(what, out, want);
}

/* Software and AES-NI SDCTR agree across the low 64 bits' wrap. */
static void ctr_carry(void)
{
    unsigned char k[16], v[16], sw[48] = {0}, ni[48] = {0};
    unhex(KEY128, k);
    unhex("0123456789abcdefffffffffffffffff", v);
    ssh_cipher *a = ssh_cipher_new(&ssh_aes128_sdctr_sw);
    ssh_cipher *b = ssh_cipher_new(&ssh_aes128_sdctr_ni);
    ssh_cipher_setkey(a, k); ssh_cipher_setiv(a, v); ssh_cipher_encrypt(a, sw, 48);
    ssh_cipher_setkey(b, k); ssh_cipher_setiv(b, v); ssh_cipher_encrypt(b, ni, 48);
    if (memcmp(sw, ni, 48) != 0) {
        printf("FAIL aes128-ctr: AES-NI and software disagree across the carry\n");
        failures++;
    }
    ssh_cipher_free(a);
    ssh_cipher_free(b);
}

int main(void)
{
    const char *ctr128 =
        "874d6191b620e3261bef6864990db6ce9806f66b7970fdff8617187bb9fffdff";
    const char *ctr256 =
        "601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c5";
    const char *cbc128 =
        "7649abac8119b246cee98e9b12e9197d5086cb9b507219ee95db113a917678b2";

    cipher("aes128-ctr sw", &ssh_aes128_sdctr_sw, KEY128, CTR_IV, ctr128, false);
    cipher("aes256-ctr sw", &ssh_aes256_sdctr_sw, KEY256, CTR_IV, ctr256, false);
    cipher("aes128-cbc sw", &ssh_aes128_cbc_sw, KEY128, CBC_IV, cbc128, true);
    if (aes_ni_available()) {
        cipher("aes128-ctr ni", &ssh_aes128_sdctr_ni, KEY128, CTR_IV, ctr128, false);
        cipher("aes256-ctr ni", &ssh_aes256_sdctr_ni, KEY256, CTR_IV, ctr256, false);
        cipher("aes128-cbc ni", &ssh_aes128_cbc_ni, KEY128, CBC_IV, cbc128, true);
        ctr_carry();
    } else {
        printf("note: no AES-NI on this CPU; hardware AES not checked\n");
    }
    /* What a session actually gets: the selector's choice. */
    cipher("aes128-ctr selected", &ssh_aes128_sdctr, KEY128, CTR_IV, ctr128, false);

    hash("sha1", &ssh_sha1, "a9993e364706816aba3e25717850c26c9cd0d89d");
    hash("sha256 sw", &ssh_sha256_sw,
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    hash("sha256 selected", &ssh_sha256,
         "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    hash("sha512", &ssh_sha512,
         "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
         "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");

    printf(failures ? "%d failure(s)\n" : "OK\n", failures);
    return failures ? 1 : 0;
}
