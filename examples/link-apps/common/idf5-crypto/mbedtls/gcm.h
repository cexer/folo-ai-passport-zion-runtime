#pragma once
/* Passport OS's original Muse/Noise code includes the Mbed TLS 3 public GCM
 * path. IDF 6 moves this API to TF-PSA's private header. Use the current SDK's
 * actual declarations/layout, never an IDF 5 context definition. Packages stay
 * bound to the exact host SDK contract. */
#ifndef MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#endif
#include "mbedtls/private/gcm.h"
