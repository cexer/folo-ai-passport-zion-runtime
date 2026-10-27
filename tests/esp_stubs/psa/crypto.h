#pragma once
/* Host-only PSA hashing surface over the existing real SHA-256 test code.
 * No key storage, entropy or hardware-driver behavior is simulated here. */
#include "mbedtls/sha256.h"
typedef int32_t psa_status_t;
typedef uint32_t psa_algorithm_t;
#define PSA_SUCCESS ((psa_status_t)0)
#define PSA_ERROR_INVALID_ARGUMENT ((psa_status_t)-135)
#define PSA_ERROR_BAD_STATE ((psa_status_t)-137)
#define PSA_ERROR_BUFFER_TOO_SMALL ((psa_status_t)-138)
#define PSA_ALG_SHA_256 ((psa_algorithm_t)0x02000009)
typedef struct {mbedtls_sha256_context context;bool active;} psa_hash_operation_t;
#define PSA_HASH_OPERATION_INIT {0}
static inline psa_status_t psa_crypto_init(void) {return PSA_SUCCESS;}
static inline psa_status_t psa_hash_setup(psa_hash_operation_t *op,psa_algorithm_t algorithm) {
    if(algorithm!=PSA_ALG_SHA_256) return PSA_ERROR_INVALID_ARGUMENT;
    if(op->active) return PSA_ERROR_BAD_STATE;
    mbedtls_sha256_init(&op->context);op->active=true;
    return mbedtls_sha256_starts(&op->context,0);
}
static inline psa_status_t psa_hash_update(psa_hash_operation_t *op,const uint8_t *input,size_t bytes) {
    if(!op->active) return PSA_ERROR_BAD_STATE;
    return mbedtls_sha256_update(&op->context,input,bytes);
}
static inline psa_status_t psa_hash_finish(psa_hash_operation_t *op,uint8_t *out,size_t capacity,size_t *length) {
    if(!op->active) return PSA_ERROR_BAD_STATE;
    if(capacity<32) return PSA_ERROR_BUFFER_TOO_SMALL;
    int status=mbedtls_sha256_finish(&op->context,out);op->active=false;*length=32;return status;
}
static inline psa_status_t psa_hash_abort(psa_hash_operation_t *op) {
    mbedtls_sha256_free(&op->context);op->active=false;return PSA_SUCCESS;
}
