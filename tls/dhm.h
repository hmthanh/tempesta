/**
 * Diffie-Hellman-Merkle key exchange.
 *
 * <em>RFC-3526: More Modular Exponential (MODP) Diffie-Hellman groups for
 * Internet Key Exchange (IKE)</em> defines a number of standardized
 * Diffie-Hellman groups for IKE.
 *
 * <em>RFC-5114: Additional Diffie-Hellman Groups for Use with IETF
 * Standards</em> defines a number of standardized Diffie-Hellman
 * groups that can be used.
 *
 * \warning  The security of the DHM key exchange relies on the proper choice
 *		   of prime modulus - optimally, it should be a safe prime. The usage
 *		   of non-safe primes both decreases the difficulty of the underlying
 *		   discrete logarithm problem and can lead to small subgroup attacks
 *		   leaking private exponent bits when invalid public keys are used
 *		   and not detected. This is especially relevant if the same DHM
 *		   parameters are reused for multiple key exchanges as in static DHM,
 *		   while the criticality of small-subgroup attacks is lower for
 *		   ephemeral DHM.
 *
 * \warning  For performance reasons, the code does neither perform primality
 *		   nor safe primality tests, nor the expensive checks for invalid
 *		   subgroups. Moreover, even if these were performed, non-standardized
 *		   primes cannot be trusted because of the possibility of backdoors
 *		   that can't be effectively checked for.
 *
 * \warning  Diffie-Hellman-Merkle is therefore a security risk when not using
 *		   standardized primes generated using a trustworthy ("nothing up
 *		   my sleeve") method, such as the RFC 3526 / 7919 primes. In the TLS
 *		   protocol, DH parameters need to be negotiated, so using the default
 *		   primes systematically is not always an option. If possible, use
 *		   Elliptic Curve Diffie-Hellman (ECDH), which has better performance,
 *		   and for which the TLS protocol mandates the use of standard
 *		   parameters.
 *
 */
/*
 *  Copyright (C) 2006-2018, Arm Limited (or its affiliates), All Rights Reserved
 *  SPDX-License-Identifier: GPL-2.0
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  This file is part of Mbed TLS (https://tls.mbed.org)
 */
#ifndef TTLS_DHM_H
#define TTLS_DHM_H

#include "config.h"
#include "bignum.h"

/*
 * DHM Error codes
 */
#define TTLS_ERR_DHM_BAD_INPUT_DATA		-0x3080  /**< Bad input parameters. */
#define TTLS_ERR_DHM_READ_PARAMS_FAILED				-0x3100  /**< Reading of the DHM parameters failed. */
#define TTLS_ERR_DHM_MAKE_PARAMS_FAILED				-0x3180  /**< Making of the DHM parameters failed. */
#define TTLS_ERR_DHM_READ_PUBLIC_FAILED				-0x3200  /**< Reading of the public values failed. */
#define TTLS_ERR_DHM_MAKE_PUBLIC_FAILED				-0x3280  /**< Making of the public value failed. */
#define TTLS_ERR_DHM_CALC_SECRET_FAILED				-0x3300  /**< Calculation of the DHM secret failed. */
#define TTLS_ERR_DHM_INVALID_FORMAT		-0x3380  /**< The ASN.1 data is not formatted correctly. */
#define TTLS_ERR_DHM_ALLOC_FAILED		  -0x3400  /**< Allocation of memory failed. */
#define TTLS_ERR_DHM_FILE_IO_ERROR		 -0x3480  /**< Read or write of file failed. */
#define TTLS_ERR_DHM_HW_ACCEL_FAILED				   -0x3500  /**< DHM hardware accelerator failed. */
#define TTLS_ERR_DHM_SET_GROUP_FAILED				  -0x3580  /**< Setting the modulus and generator failed. */

/**
 * \brief		  The DHM context structure.
 */
typedef struct
{
	size_t len;		 /*!<  The size of \p P in Bytes. */
	ttls_mpi P;	  /*!<  The prime modulus. */
	ttls_mpi G;	  /*!<  The generator. */
	ttls_mpi X;	  /*!<  Our secret value. */
	ttls_mpi GX;	 /*!<  Our public key = \c G^X mod \c P. */
	ttls_mpi GY;	 /*!<  The public key of the peer = \c G^Y mod \c P. */
	ttls_mpi K;	  /*!<  The shared secret = \c G^(XY) mod \c P. */
	ttls_mpi RP;	 /*!<  The cached value = \c R^2 mod \c P. */
	ttls_mpi Vi;	 /*!<  The blinding value. */
	ttls_mpi Vf;	 /*!<  The unblinding value. */
	ttls_mpi pX;	 /*!<  The previous \c X. */
}
ttls_dhm_context;

/**
 * \brief		  This function initializes the DHM context.
 *
 * \param ctx	  The DHM context to initialize.
 */
void ttls_dhm_init(ttls_dhm_context *ctx);

/**
 * \brief		  This function parses the ServerKeyExchange parameters.
 *
 * \param ctx	  The DHM context.
 * \param p		On input, *p must be the start of the input buffer.
 *				 On output, *p is updated to point to the end of the data
 *				 that has been read. On success, this is the first byte
 *				 past the end of the ServerKeyExchange parameters.
 *				 On error, this is the point at which an error has been
 *				 detected, which is usually not useful except to debug
 *				 failures.
 * \param end	  The end of the input buffer.
 *
 * \return		 \c 0 on success, or an \c TTLS_ERR_DHM_XXX error code
 *				 on failure.
 */
int ttls_dhm_read_params(ttls_dhm_context *ctx,
		 unsigned char **p,
		 const unsigned char *end);

/**
 * \brief  This function sets up and writes the ServerKeyExchange
 *		 parameters.
 *
 * \param ctx	  The DHM context.
 * \param x_size   The private value size in Bytes.
 * \param olen	 The number of characters written.
 * \param output   The destination buffer.
 *
 * \note   The destination buffer must be large enough to hold
 *		 the reduced binary presentation of the modulus, the generator
 *		 and the public key, each wrapped with a 2-byte length field.
 *		 It is the responsibility of the caller to ensure that enough
 *		 space is available. Refer to \c ttls_mpi_size to computing
 *		 the byte-size of an MPI.
 *
 * \note  This function assumes that \c ctx->P and \c ctx->G
 *		 have already been properly set. For that, use
 *		 ttls_dhm_set_group() below in conjunction with
 *		 ttls_mpi_read_binary() and ttls_mpi_read_string().
 *
 * \return	 \c 0 on success, or an \c TTLS_ERR_DHM_XXX error code
 *			 on failure.
 */
int ttls_dhm_make_params(ttls_dhm_context *ctx, int x_size,
			 unsigned char *output, size_t *olen);

/**
 * \brief		  Set prime modulus and generator
 *
 * \param ctx	  The DHM context.
 * \param P		The MPI holding DHM prime modulus.
 * \param G		The MPI holding DHM generator.
 *
 * \note		   This function can be used to set P, G
 *				 in preparation for \c ttls_dhm_make_params.
 *
 * \return		 \c 0 if successful, or an \c TTLS_ERR_DHM_XXX error code
 *				 on failure.
 */
int ttls_dhm_set_group(ttls_dhm_context *ctx,
			   const ttls_mpi *P,
			   const ttls_mpi *G);

/**
 * \brief		  This function imports the public value G^Y of the peer.
 *
 * \param ctx	  The DHM context.
 * \param input	The input buffer.
 * \param ilen	 The size of the input buffer.
 *
 * \return		 \c 0 on success, or an \c TTLS_ERR_DHM_XXX error code
 *				 on failure.
 */
int ttls_dhm_read_public(ttls_dhm_context *ctx,
		 const unsigned char *input, size_t ilen);

/**
 * \brief		  This function creates its own private value \c X and
 *				 exports \c G^X.
 *
 * \param ctx	  The DHM context.
 * \param x_size   The private value size in Bytes.
 * \param output   The destination buffer.
 * \param olen	 The length of the destination buffer. Must be at least
				   equal to ctx->len (the size of \c P).
 *
 * \note		   The destination buffer will always be fully written
 *				 so as to contain a big-endian presentation of G^X mod P.
 *				 If it is larger than ctx->len, it will accordingly be
 *				 padded with zero-bytes in the beginning.
 *
 * \return		 \c 0 on success, or an \c TTLS_ERR_DHM_XXX error code
 *				 on failure.
 */
int ttls_dhm_make_public(ttls_dhm_context *ctx, int x_size,
		 unsigned char *output, size_t olen);

/**
 * \brief			   This function derives and exports the shared secret
 *		  \c (G^Y)^X mod \c P.
 *
 * \param ctx		   The DHM context.
 * \param output		The destination buffer.
 * \param output_size   The size of the destination buffer. Must be at least
 *		  the size of ctx->len.
 * \param olen		  On exit, holds the actual number of Bytes written.
 *
 * \return		 \c 0 on success, or an \c TTLS_ERR_DHM_XXX error code
 *				 on failure.
 *
 * \note		   If non-NULL, \p f_rng is used to blind the input as
 *				 a countermeasure against timing attacks. Blinding is used
 *				 only if our secret value \p X is re-used and omitted
 *				 otherwise. Therefore, we recommend always passing a
 *				 non-NULL \p f_rng argument.
 */
int ttls_dhm_calc_secret(ttls_dhm_context *ctx,
		 unsigned char *output, size_t output_size, size_t *olen);

/**
 * RFC 3526, RFC 5114 and RFC 7919 standardize a number of
 * Diffie-Hellman groups, some of which are included here
 * for use within the SSL/TLS module and the user's convenience
 * when configuring the Diffie-Hellman parameters by hand
 * through \c ttls_conf_dh_param.
 *
 * The following lists the source of the above groups in the standards:
 * - RFC 5114 section 2.2:  2048-bit MODP Group with 224-bit Prime Order Subgroup
 * - RFC 3526 section 3:	2048-bit MODP Group
 * - RFC 3526 section 4:	3072-bit MODP Group
 * - RFC 3526 section 5:	4096-bit MODP Group
 * - RFC 7919 section A.1:  ffdhe2048
 * - RFC 7919 section A.2:  ffdhe3072
 * - RFC 7919 section A.3:  ffdhe4096
 * - RFC 7919 section A.4:  ffdhe6144
 * - RFC 7919 section A.5:  ffdhe8192
 *
 * The constants with suffix "_p" denote the chosen prime moduli, while
 * the constants with suffix "_g" denote the chosen generator
 * of the associated prime field.
 *
 * The constants further suffixed with "_bin" are provided in binary format,
 * while all other constants represent null-terminated strings holding the
 * hexadecimal presentation of the respective numbers.
 *
 * The primes from RFC 3526 and RFC 7919 have been generating by the following
 * trust-worthy procedure:
 * - Fix N in { 2048, 3072, 4096, 6144, 8192 } and consider the N-bit number
 *   the first and last 64 bits are all 1, and the remaining N - 128 bits of
 *   which are 0x7ff...ff.
 * - Add the smallest multiple of the first N - 129 bits of the binary expansion
 *   of pi (for RFC 5236) or e (for RFC 7919) to this intermediate bit-string
 *   such that the resulting integer is a safe-prime.
 * - The result is the respective RFC 3526 / 7919 prime, and the corresponding
 *   generator is always chosen to be 2 (which is a square for these prime,
 *   hence the corresponding subgroup has order (p-1)/2 and avoids leaking a
 *   bit in the private exponent).
 *
 */

/*
 * Trustworthy DHM parameters in binary form
 */

#define TTLS_DHM_RFC3526_MODP_2048_P_BIN {		\
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	 0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34, \
	 0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, \
	 0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, \
	 0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, \
	 0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD, \
	 0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, \
	 0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37, \
	 0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, \
	 0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, \
	 0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, \
	 0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED, \
	 0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, \
	 0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6, \
	 0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D, \
	 0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, \
	 0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A, \
	 0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F, \
	 0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, \
	 0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB, \
	 0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D, \
	 0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, \
	 0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C, \
	 0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B, \
	 0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, \
	 0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F, \
	 0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9, \
	 0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, \
	 0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5, \
	 0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10, \
	 0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAC, 0xAA, 0x68, \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC3526_MODP_2048_G_BIN { 0x02 }

#define TTLS_DHM_RFC3526_MODP_3072_P_BIN {	   \
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34, \
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1, \
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74, \
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22, \
	0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD, \
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B, \
	0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37, \
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45, \
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6, \
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B, \
	0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED, \
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5, \
	0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6, \
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D, \
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05, \
	0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A, \
	0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F, \
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96, \
	0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB, \
	0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D, \
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04, \
	0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C, \
	0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B, \
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03, \
	0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F, \
	0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9, \
	0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18, \
	0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5, \
	0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10, \
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D, \
	0xAD, 0x33, 0x17, 0x0D, 0x04, 0x50, 0x7A, 0x33, \
	0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64, \
	0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A, \
	0x8A, 0xEA, 0x71, 0x57, 0x5D, 0x06, 0x0C, 0x7D, \
	0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7, \
	0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7, \
	0x1E, 0x8C, 0x94, 0xE0, 0x4A, 0x25, 0x61, 0x9D, \
	0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B, \
	0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64, \
	0xD8, 0x76, 0x02, 0x73, 0x3E, 0xC8, 0x6A, 0x64, \
	0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C, \
	0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C, \
	0x77, 0x09, 0x88, 0xC0, 0xBA, 0xD9, 0x46, 0xE2, \
	0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31, \
	0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E, \
	0x4B, 0x82, 0xD1, 0x20, 0xA9, 0x3A, 0xD2, 0xCA, \
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC3526_MODP_3072_G_BIN { 0x02 }

#define TTLS_DHM_RFC3526_MODP_4096_P_BIN  {	   \
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  \
	0xC9, 0x0F, 0xDA, 0xA2, 0x21, 0x68, 0xC2, 0x34,  \
	0xC4, 0xC6, 0x62, 0x8B, 0x80, 0xDC, 0x1C, 0xD1,  \
	0x29, 0x02, 0x4E, 0x08, 0x8A, 0x67, 0xCC, 0x74,  \
	0x02, 0x0B, 0xBE, 0xA6, 0x3B, 0x13, 0x9B, 0x22,  \
	0x51, 0x4A, 0x08, 0x79, 0x8E, 0x34, 0x04, 0xDD,  \
	0xEF, 0x95, 0x19, 0xB3, 0xCD, 0x3A, 0x43, 0x1B,  \
	0x30, 0x2B, 0x0A, 0x6D, 0xF2, 0x5F, 0x14, 0x37,  \
	0x4F, 0xE1, 0x35, 0x6D, 0x6D, 0x51, 0xC2, 0x45,  \
	0xE4, 0x85, 0xB5, 0x76, 0x62, 0x5E, 0x7E, 0xC6,  \
	0xF4, 0x4C, 0x42, 0xE9, 0xA6, 0x37, 0xED, 0x6B,  \
	0x0B, 0xFF, 0x5C, 0xB6, 0xF4, 0x06, 0xB7, 0xED,  \
	0xEE, 0x38, 0x6B, 0xFB, 0x5A, 0x89, 0x9F, 0xA5,  \
	0xAE, 0x9F, 0x24, 0x11, 0x7C, 0x4B, 0x1F, 0xE6,  \
	0x49, 0x28, 0x66, 0x51, 0xEC, 0xE4, 0x5B, 0x3D,  \
	0xC2, 0x00, 0x7C, 0xB8, 0xA1, 0x63, 0xBF, 0x05,  \
	0x98, 0xDA, 0x48, 0x36, 0x1C, 0x55, 0xD3, 0x9A,  \
	0x69, 0x16, 0x3F, 0xA8, 0xFD, 0x24, 0xCF, 0x5F,  \
	0x83, 0x65, 0x5D, 0x23, 0xDC, 0xA3, 0xAD, 0x96,  \
	0x1C, 0x62, 0xF3, 0x56, 0x20, 0x85, 0x52, 0xBB,  \
	0x9E, 0xD5, 0x29, 0x07, 0x70, 0x96, 0x96, 0x6D,  \
	0x67, 0x0C, 0x35, 0x4E, 0x4A, 0xBC, 0x98, 0x04,  \
	0xF1, 0x74, 0x6C, 0x08, 0xCA, 0x18, 0x21, 0x7C,  \
	0x32, 0x90, 0x5E, 0x46, 0x2E, 0x36, 0xCE, 0x3B,  \
	0xE3, 0x9E, 0x77, 0x2C, 0x18, 0x0E, 0x86, 0x03,  \
	0x9B, 0x27, 0x83, 0xA2, 0xEC, 0x07, 0xA2, 0x8F,  \
	0xB5, 0xC5, 0x5D, 0xF0, 0x6F, 0x4C, 0x52, 0xC9,  \
	0xDE, 0x2B, 0xCB, 0xF6, 0x95, 0x58, 0x17, 0x18,  \
	0x39, 0x95, 0x49, 0x7C, 0xEA, 0x95, 0x6A, 0xE5,  \
	0x15, 0xD2, 0x26, 0x18, 0x98, 0xFA, 0x05, 0x10,  \
	0x15, 0x72, 0x8E, 0x5A, 0x8A, 0xAA, 0xC4, 0x2D,  \
	0xAD, 0x33, 0x17, 0x0D, 0x04, 0x50, 0x7A, 0x33,  \
	0xA8, 0x55, 0x21, 0xAB, 0xDF, 0x1C, 0xBA, 0x64,  \
	0xEC, 0xFB, 0x85, 0x04, 0x58, 0xDB, 0xEF, 0x0A,  \
	0x8A, 0xEA, 0x71, 0x57, 0x5D, 0x06, 0x0C, 0x7D,  \
	0xB3, 0x97, 0x0F, 0x85, 0xA6, 0xE1, 0xE4, 0xC7,  \
	0xAB, 0xF5, 0xAE, 0x8C, 0xDB, 0x09, 0x33, 0xD7,  \
	0x1E, 0x8C, 0x94, 0xE0, 0x4A, 0x25, 0x61, 0x9D,  \
	0xCE, 0xE3, 0xD2, 0x26, 0x1A, 0xD2, 0xEE, 0x6B,  \
	0xF1, 0x2F, 0xFA, 0x06, 0xD9, 0x8A, 0x08, 0x64,  \
	0xD8, 0x76, 0x02, 0x73, 0x3E, 0xC8, 0x6A, 0x64,  \
	0x52, 0x1F, 0x2B, 0x18, 0x17, 0x7B, 0x20, 0x0C,  \
	0xBB, 0xE1, 0x17, 0x57, 0x7A, 0x61, 0x5D, 0x6C,  \
	0x77, 0x09, 0x88, 0xC0, 0xBA, 0xD9, 0x46, 0xE2,  \
	0x08, 0xE2, 0x4F, 0xA0, 0x74, 0xE5, 0xAB, 0x31,  \
	0x43, 0xDB, 0x5B, 0xFC, 0xE0, 0xFD, 0x10, 0x8E,  \
	0x4B, 0x82, 0xD1, 0x20, 0xA9, 0x21, 0x08, 0x01,  \
	0x1A, 0x72, 0x3C, 0x12, 0xA7, 0x87, 0xE6, 0xD7,  \
	0x88, 0x71, 0x9A, 0x10, 0xBD, 0xBA, 0x5B, 0x26,  \
	0x99, 0xC3, 0x27, 0x18, 0x6A, 0xF4, 0xE2, 0x3C,  \
	0x1A, 0x94, 0x68, 0x34, 0xB6, 0x15, 0x0B, 0xDA,  \
	0x25, 0x83, 0xE9, 0xCA, 0x2A, 0xD4, 0x4C, 0xE8,  \
	0xDB, 0xBB, 0xC2, 0xDB, 0x04, 0xDE, 0x8E, 0xF9,  \
	0x2E, 0x8E, 0xFC, 0x14, 0x1F, 0xBE, 0xCA, 0xA6,  \
	0x28, 0x7C, 0x59, 0x47, 0x4E, 0x6B, 0xC0, 0x5D,  \
	0x99, 0xB2, 0x96, 0x4F, 0xA0, 0x90, 0xC3, 0xA2,  \
	0x23, 0x3B, 0xA1, 0x86, 0x51, 0x5B, 0xE7, 0xED,  \
	0x1F, 0x61, 0x29, 0x70, 0xCE, 0xE2, 0xD7, 0xAF,  \
	0xB8, 0x1B, 0xDD, 0x76, 0x21, 0x70, 0x48, 0x1C,  \
	0xD0, 0x06, 0x91, 0x27, 0xD5, 0xB0, 0x5A, 0xA9,  \
	0x93, 0xB4, 0xEA, 0x98, 0x8D, 0x8F, 0xDD, 0xC1,  \
	0x86, 0xFF, 0xB7, 0xDC, 0x90, 0xA6, 0xC0, 0x8F,  \
	0x4D, 0xF4, 0x35, 0xC9, 0x34, 0x06, 0x31, 0x99,  \
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC3526_MODP_4096_G_BIN { 0x02 }

#define TTLS_DHM_RFC7919_FFDHE2048_P_BIN {		\
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	 0xAD, 0xF8, 0x54, 0x58, 0xA2, 0xBB, 0x4A, 0x9A, \
	 0xAF, 0xDC, 0x56, 0x20, 0x27, 0x3D, 0x3C, 0xF1, \
	 0xD8, 0xB9, 0xC5, 0x83, 0xCE, 0x2D, 0x36, 0x95, \
	 0xA9, 0xE1, 0x36, 0x41, 0x14, 0x64, 0x33, 0xFB, \
	 0xCC, 0x93, 0x9D, 0xCE, 0x24, 0x9B, 0x3E, 0xF9, \
	 0x7D, 0x2F, 0xE3, 0x63, 0x63, 0x0C, 0x75, 0xD8, \
	 0xF6, 0x81, 0xB2, 0x02, 0xAE, 0xC4, 0x61, 0x7A, \
	 0xD3, 0xDF, 0x1E, 0xD5, 0xD5, 0xFD, 0x65, 0x61, \
	 0x24, 0x33, 0xF5, 0x1F, 0x5F, 0x06, 0x6E, 0xD0, \
	 0x85, 0x63, 0x65, 0x55, 0x3D, 0xED, 0x1A, 0xF3, \
	 0xB5, 0x57, 0x13, 0x5E, 0x7F, 0x57, 0xC9, 0x35, \
	 0x98, 0x4F, 0x0C, 0x70, 0xE0, 0xE6, 0x8B, 0x77, \
	 0xE2, 0xA6, 0x89, 0xDA, 0xF3, 0xEF, 0xE8, 0x72, \
	 0x1D, 0xF1, 0x58, 0xA1, 0x36, 0xAD, 0xE7, 0x35, \
	 0x30, 0xAC, 0xCA, 0x4F, 0x48, 0x3A, 0x79, 0x7A, \
	 0xBC, 0x0A, 0xB1, 0x82, 0xB3, 0x24, 0xFB, 0x61, \
	 0xD1, 0x08, 0xA9, 0x4B, 0xB2, 0xC8, 0xE3, 0xFB, \
	 0xB9, 0x6A, 0xDA, 0xB7, 0x60, 0xD7, 0xF4, 0x68, \
	 0x1D, 0x4F, 0x42, 0xA3, 0xDE, 0x39, 0x4D, 0xF4, \
	 0xAE, 0x56, 0xED, 0xE7, 0x63, 0x72, 0xBB, 0x19, \
	 0x0B, 0x07, 0xA7, 0xC8, 0xEE, 0x0A, 0x6D, 0x70, \
	 0x9E, 0x02, 0xFC, 0xE1, 0xCD, 0xF7, 0xE2, 0xEC, \
	 0xC0, 0x34, 0x04, 0xCD, 0x28, 0x34, 0x2F, 0x61, \
	 0x91, 0x72, 0xFE, 0x9C, 0xE9, 0x85, 0x83, 0xFF, \
	 0x8E, 0x4F, 0x12, 0x32, 0xEE, 0xF2, 0x81, 0x83, \
	 0xC3, 0xFE, 0x3B, 0x1B, 0x4C, 0x6F, 0xAD, 0x73, \
	 0x3B, 0xB5, 0xFC, 0xBC, 0x2E, 0xC2, 0x20, 0x05, \
	 0xC5, 0x8E, 0xF1, 0x83, 0x7D, 0x16, 0x83, 0xB2, \
	 0xC6, 0xF3, 0x4A, 0x26, 0xC1, 0xB2, 0xEF, 0xFA, \
	 0x88, 0x6B, 0x42, 0x38, 0x61, 0x28, 0x5C, 0x97, \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, }

#define TTLS_DHM_RFC7919_FFDHE2048_G_BIN { 0x02 }

#define TTLS_DHM_RFC7919_FFDHE3072_P_BIN { \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	 0xAD, 0xF8, 0x54, 0x58, 0xA2, 0xBB, 0x4A, 0x9A, \
	 0xAF, 0xDC, 0x56, 0x20, 0x27, 0x3D, 0x3C, 0xF1, \
	 0xD8, 0xB9, 0xC5, 0x83, 0xCE, 0x2D, 0x36, 0x95, \
	 0xA9, 0xE1, 0x36, 0x41, 0x14, 0x64, 0x33, 0xFB, \
	 0xCC, 0x93, 0x9D, 0xCE, 0x24, 0x9B, 0x3E, 0xF9, \
	 0x7D, 0x2F, 0xE3, 0x63, 0x63, 0x0C, 0x75, 0xD8, \
	 0xF6, 0x81, 0xB2, 0x02, 0xAE, 0xC4, 0x61, 0x7A, \
	 0xD3, 0xDF, 0x1E, 0xD5, 0xD5, 0xFD, 0x65, 0x61, \
	 0x24, 0x33, 0xF5, 0x1F, 0x5F, 0x06, 0x6E, 0xD0, \
	 0x85, 0x63, 0x65, 0x55, 0x3D, 0xED, 0x1A, 0xF3, \
	 0xB5, 0x57, 0x13, 0x5E, 0x7F, 0x57, 0xC9, 0x35, \
	 0x98, 0x4F, 0x0C, 0x70, 0xE0, 0xE6, 0x8B, 0x77, \
	 0xE2, 0xA6, 0x89, 0xDA, 0xF3, 0xEF, 0xE8, 0x72, \
	 0x1D, 0xF1, 0x58, 0xA1, 0x36, 0xAD, 0xE7, 0x35, \
	 0x30, 0xAC, 0xCA, 0x4F, 0x48, 0x3A, 0x79, 0x7A, \
	 0xBC, 0x0A, 0xB1, 0x82, 0xB3, 0x24, 0xFB, 0x61, \
	 0xD1, 0x08, 0xA9, 0x4B, 0xB2, 0xC8, 0xE3, 0xFB, \
	 0xB9, 0x6A, 0xDA, 0xB7, 0x60, 0xD7, 0xF4, 0x68, \
	 0x1D, 0x4F, 0x42, 0xA3, 0xDE, 0x39, 0x4D, 0xF4, \
	 0xAE, 0x56, 0xED, 0xE7, 0x63, 0x72, 0xBB, 0x19, \
	 0x0B, 0x07, 0xA7, 0xC8, 0xEE, 0x0A, 0x6D, 0x70, \
	 0x9E, 0x02, 0xFC, 0xE1, 0xCD, 0xF7, 0xE2, 0xEC, \
	 0xC0, 0x34, 0x04, 0xCD, 0x28, 0x34, 0x2F, 0x61, \
	 0x91, 0x72, 0xFE, 0x9C, 0xE9, 0x85, 0x83, 0xFF, \
	 0x8E, 0x4F, 0x12, 0x32, 0xEE, 0xF2, 0x81, 0x83, \
	 0xC3, 0xFE, 0x3B, 0x1B, 0x4C, 0x6F, 0xAD, 0x73, \
	 0x3B, 0xB5, 0xFC, 0xBC, 0x2E, 0xC2, 0x20, 0x05, \
	 0xC5, 0x8E, 0xF1, 0x83, 0x7D, 0x16, 0x83, 0xB2, \
	 0xC6, 0xF3, 0x4A, 0x26, 0xC1, 0xB2, 0xEF, 0xFA, \
	 0x88, 0x6B, 0x42, 0x38, 0x61, 0x1F, 0xCF, 0xDC, \
	 0xDE, 0x35, 0x5B, 0x3B, 0x65, 0x19, 0x03, 0x5B, \
	 0xBC, 0x34, 0xF4, 0xDE, 0xF9, 0x9C, 0x02, 0x38, \
	 0x61, 0xB4, 0x6F, 0xC9, 0xD6, 0xE6, 0xC9, 0x07, \
	 0x7A, 0xD9, 0x1D, 0x26, 0x91, 0xF7, 0xF7, 0xEE, \
	 0x59, 0x8C, 0xB0, 0xFA, 0xC1, 0x86, 0xD9, 0x1C, \
	 0xAE, 0xFE, 0x13, 0x09, 0x85, 0x13, 0x92, 0x70, \
	 0xB4, 0x13, 0x0C, 0x93, 0xBC, 0x43, 0x79, 0x44, \
	 0xF4, 0xFD, 0x44, 0x52, 0xE2, 0xD7, 0x4D, 0xD3, \
	 0x64, 0xF2, 0xE2, 0x1E, 0x71, 0xF5, 0x4B, 0xFF, \
	 0x5C, 0xAE, 0x82, 0xAB, 0x9C, 0x9D, 0xF6, 0x9E, \
	 0xE8, 0x6D, 0x2B, 0xC5, 0x22, 0x36, 0x3A, 0x0D, \
	 0xAB, 0xC5, 0x21, 0x97, 0x9B, 0x0D, 0xEA, 0xDA, \
	 0x1D, 0xBF, 0x9A, 0x42, 0xD5, 0xC4, 0x48, 0x4E, \
	 0x0A, 0xBC, 0xD0, 0x6B, 0xFA, 0x53, 0xDD, 0xEF, \
	 0x3C, 0x1B, 0x20, 0xEE, 0x3F, 0xD5, 0x9D, 0x7C, \
	 0x25, 0xE4, 0x1D, 0x2B, 0x66, 0xC6, 0x2E, 0x37, \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC7919_FFDHE3072_G_BIN { 0x02 }

#define TTLS_DHM_RFC7919_FFDHE4096_P_BIN {		\
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	 0xAD, 0xF8, 0x54, 0x58, 0xA2, 0xBB, 0x4A, 0x9A, \
	 0xAF, 0xDC, 0x56, 0x20, 0x27, 0x3D, 0x3C, 0xF1, \
	 0xD8, 0xB9, 0xC5, 0x83, 0xCE, 0x2D, 0x36, 0x95, \
	 0xA9, 0xE1, 0x36, 0x41, 0x14, 0x64, 0x33, 0xFB, \
	 0xCC, 0x93, 0x9D, 0xCE, 0x24, 0x9B, 0x3E, 0xF9, \
	 0x7D, 0x2F, 0xE3, 0x63, 0x63, 0x0C, 0x75, 0xD8, \
	 0xF6, 0x81, 0xB2, 0x02, 0xAE, 0xC4, 0x61, 0x7A, \
	 0xD3, 0xDF, 0x1E, 0xD5, 0xD5, 0xFD, 0x65, 0x61, \
	 0x24, 0x33, 0xF5, 0x1F, 0x5F, 0x06, 0x6E, 0xD0, \
	 0x85, 0x63, 0x65, 0x55, 0x3D, 0xED, 0x1A, 0xF3, \
	 0xB5, 0x57, 0x13, 0x5E, 0x7F, 0x57, 0xC9, 0x35, \
	 0x98, 0x4F, 0x0C, 0x70, 0xE0, 0xE6, 0x8B, 0x77, \
	 0xE2, 0xA6, 0x89, 0xDA, 0xF3, 0xEF, 0xE8, 0x72, \
	 0x1D, 0xF1, 0x58, 0xA1, 0x36, 0xAD, 0xE7, 0x35, \
	 0x30, 0xAC, 0xCA, 0x4F, 0x48, 0x3A, 0x79, 0x7A, \
	 0xBC, 0x0A, 0xB1, 0x82, 0xB3, 0x24, 0xFB, 0x61, \
	 0xD1, 0x08, 0xA9, 0x4B, 0xB2, 0xC8, 0xE3, 0xFB, \
	 0xB9, 0x6A, 0xDA, 0xB7, 0x60, 0xD7, 0xF4, 0x68, \
	 0x1D, 0x4F, 0x42, 0xA3, 0xDE, 0x39, 0x4D, 0xF4, \
	 0xAE, 0x56, 0xED, 0xE7, 0x63, 0x72, 0xBB, 0x19, \
	 0x0B, 0x07, 0xA7, 0xC8, 0xEE, 0x0A, 0x6D, 0x70, \
	 0x9E, 0x02, 0xFC, 0xE1, 0xCD, 0xF7, 0xE2, 0xEC, \
	 0xC0, 0x34, 0x04, 0xCD, 0x28, 0x34, 0x2F, 0x61, \
	 0x91, 0x72, 0xFE, 0x9C, 0xE9, 0x85, 0x83, 0xFF, \
	 0x8E, 0x4F, 0x12, 0x32, 0xEE, 0xF2, 0x81, 0x83, \
	 0xC3, 0xFE, 0x3B, 0x1B, 0x4C, 0x6F, 0xAD, 0x73, \
	 0x3B, 0xB5, 0xFC, 0xBC, 0x2E, 0xC2, 0x20, 0x05, \
	 0xC5, 0x8E, 0xF1, 0x83, 0x7D, 0x16, 0x83, 0xB2, \
	 0xC6, 0xF3, 0x4A, 0x26, 0xC1, 0xB2, 0xEF, 0xFA, \
	 0x88, 0x6B, 0x42, 0x38, 0x61, 0x1F, 0xCF, 0xDC, \
	 0xDE, 0x35, 0x5B, 0x3B, 0x65, 0x19, 0x03, 0x5B, \
	 0xBC, 0x34, 0xF4, 0xDE, 0xF9, 0x9C, 0x02, 0x38, \
	 0x61, 0xB4, 0x6F, 0xC9, 0xD6, 0xE6, 0xC9, 0x07, \
	 0x7A, 0xD9, 0x1D, 0x26, 0x91, 0xF7, 0xF7, 0xEE, \
	 0x59, 0x8C, 0xB0, 0xFA, 0xC1, 0x86, 0xD9, 0x1C, \
	 0xAE, 0xFE, 0x13, 0x09, 0x85, 0x13, 0x92, 0x70, \
	 0xB4, 0x13, 0x0C, 0x93, 0xBC, 0x43, 0x79, 0x44, \
	 0xF4, 0xFD, 0x44, 0x52, 0xE2, 0xD7, 0x4D, 0xD3, \
	 0x64, 0xF2, 0xE2, 0x1E, 0x71, 0xF5, 0x4B, 0xFF, \
	 0x5C, 0xAE, 0x82, 0xAB, 0x9C, 0x9D, 0xF6, 0x9E, \
	 0xE8, 0x6D, 0x2B, 0xC5, 0x22, 0x36, 0x3A, 0x0D, \
	 0xAB, 0xC5, 0x21, 0x97, 0x9B, 0x0D, 0xEA, 0xDA, \
	 0x1D, 0xBF, 0x9A, 0x42, 0xD5, 0xC4, 0x48, 0x4E, \
	 0x0A, 0xBC, 0xD0, 0x6B, 0xFA, 0x53, 0xDD, 0xEF, \
	 0x3C, 0x1B, 0x20, 0xEE, 0x3F, 0xD5, 0x9D, 0x7C, \
	 0x25, 0xE4, 0x1D, 0x2B, 0x66, 0x9E, 0x1E, 0xF1, \
	 0x6E, 0x6F, 0x52, 0xC3, 0x16, 0x4D, 0xF4, 0xFB, \
	 0x79, 0x30, 0xE9, 0xE4, 0xE5, 0x88, 0x57, 0xB6, \
	 0xAC, 0x7D, 0x5F, 0x42, 0xD6, 0x9F, 0x6D, 0x18, \
	 0x77, 0x63, 0xCF, 0x1D, 0x55, 0x03, 0x40, 0x04, \
	 0x87, 0xF5, 0x5B, 0xA5, 0x7E, 0x31, 0xCC, 0x7A, \
	 0x71, 0x35, 0xC8, 0x86, 0xEF, 0xB4, 0x31, 0x8A, \
	 0xED, 0x6A, 0x1E, 0x01, 0x2D, 0x9E, 0x68, 0x32, \
	 0xA9, 0x07, 0x60, 0x0A, 0x91, 0x81, 0x30, 0xC4, \
	 0x6D, 0xC7, 0x78, 0xF9, 0x71, 0xAD, 0x00, 0x38, \
	 0x09, 0x29, 0x99, 0xA3, 0x33, 0xCB, 0x8B, 0x7A, \
	 0x1A, 0x1D, 0xB9, 0x3D, 0x71, 0x40, 0x00, 0x3C, \
	 0x2A, 0x4E, 0xCE, 0xA9, 0xF9, 0x8D, 0x0A, 0xCC, \
	 0x0A, 0x82, 0x91, 0xCD, 0xCE, 0xC9, 0x7D, 0xCF, \
	 0x8E, 0xC9, 0xB5, 0x5A, 0x7F, 0x88, 0xA4, 0x6B, \
	 0x4D, 0xB5, 0xA8, 0x51, 0xF4, 0x41, 0x82, 0xE1, \
	 0xC6, 0x8A, 0x00, 0x7E, 0x5E, 0x65, 0x5F, 0x6A, \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC7919_FFDHE4096_G_BIN { 0x02 }

#define TTLS_DHM_RFC7919_FFDHE6144_P_BIN {		\
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	 0xAD, 0xF8, 0x54, 0x58, 0xA2, 0xBB, 0x4A, 0x9A, \
	 0xAF, 0xDC, 0x56, 0x20, 0x27, 0x3D, 0x3C, 0xF1, \
	 0xD8, 0xB9, 0xC5, 0x83, 0xCE, 0x2D, 0x36, 0x95, \
	 0xA9, 0xE1, 0x36, 0x41, 0x14, 0x64, 0x33, 0xFB, \
	 0xCC, 0x93, 0x9D, 0xCE, 0x24, 0x9B, 0x3E, 0xF9, \
	 0x7D, 0x2F, 0xE3, 0x63, 0x63, 0x0C, 0x75, 0xD8, \
	 0xF6, 0x81, 0xB2, 0x02, 0xAE, 0xC4, 0x61, 0x7A, \
	 0xD3, 0xDF, 0x1E, 0xD5, 0xD5, 0xFD, 0x65, 0x61, \
	 0x24, 0x33, 0xF5, 0x1F, 0x5F, 0x06, 0x6E, 0xD0, \
	 0x85, 0x63, 0x65, 0x55, 0x3D, 0xED, 0x1A, 0xF3, \
	 0xB5, 0x57, 0x13, 0x5E, 0x7F, 0x57, 0xC9, 0x35, \
	 0x98, 0x4F, 0x0C, 0x70, 0xE0, 0xE6, 0x8B, 0x77, \
	 0xE2, 0xA6, 0x89, 0xDA, 0xF3, 0xEF, 0xE8, 0x72, \
	 0x1D, 0xF1, 0x58, 0xA1, 0x36, 0xAD, 0xE7, 0x35, \
	 0x30, 0xAC, 0xCA, 0x4F, 0x48, 0x3A, 0x79, 0x7A, \
	 0xBC, 0x0A, 0xB1, 0x82, 0xB3, 0x24, 0xFB, 0x61, \
	 0xD1, 0x08, 0xA9, 0x4B, 0xB2, 0xC8, 0xE3, 0xFB, \
	 0xB9, 0x6A, 0xDA, 0xB7, 0x60, 0xD7, 0xF4, 0x68, \
	 0x1D, 0x4F, 0x42, 0xA3, 0xDE, 0x39, 0x4D, 0xF4, \
	 0xAE, 0x56, 0xED, 0xE7, 0x63, 0x72, 0xBB, 0x19, \
	 0x0B, 0x07, 0xA7, 0xC8, 0xEE, 0x0A, 0x6D, 0x70, \
	 0x9E, 0x02, 0xFC, 0xE1, 0xCD, 0xF7, 0xE2, 0xEC, \
	 0xC0, 0x34, 0x04, 0xCD, 0x28, 0x34, 0x2F, 0x61, \
	 0x91, 0x72, 0xFE, 0x9C, 0xE9, 0x85, 0x83, 0xFF, \
	 0x8E, 0x4F, 0x12, 0x32, 0xEE, 0xF2, 0x81, 0x83, \
	 0xC3, 0xFE, 0x3B, 0x1B, 0x4C, 0x6F, 0xAD, 0x73, \
	 0x3B, 0xB5, 0xFC, 0xBC, 0x2E, 0xC2, 0x20, 0x05, \
	 0xC5, 0x8E, 0xF1, 0x83, 0x7D, 0x16, 0x83, 0xB2, \
	 0xC6, 0xF3, 0x4A, 0x26, 0xC1, 0xB2, 0xEF, 0xFA, \
	 0x88, 0x6B, 0x42, 0x38, 0x61, 0x1F, 0xCF, 0xDC, \
	 0xDE, 0x35, 0x5B, 0x3B, 0x65, 0x19, 0x03, 0x5B, \
	 0xBC, 0x34, 0xF4, 0xDE, 0xF9, 0x9C, 0x02, 0x38, \
	 0x61, 0xB4, 0x6F, 0xC9, 0xD6, 0xE6, 0xC9, 0x07, \
	 0x7A, 0xD9, 0x1D, 0x26, 0x91, 0xF7, 0xF7, 0xEE, \
	 0x59, 0x8C, 0xB0, 0xFA, 0xC1, 0x86, 0xD9, 0x1C, \
	 0xAE, 0xFE, 0x13, 0x09, 0x85, 0x13, 0x92, 0x70, \
	 0xB4, 0x13, 0x0C, 0x93, 0xBC, 0x43, 0x79, 0x44, \
	 0xF4, 0xFD, 0x44, 0x52, 0xE2, 0xD7, 0x4D, 0xD3, \
	 0x64, 0xF2, 0xE2, 0x1E, 0x71, 0xF5, 0x4B, 0xFF, \
	 0x5C, 0xAE, 0x82, 0xAB, 0x9C, 0x9D, 0xF6, 0x9E, \
	 0xE8, 0x6D, 0x2B, 0xC5, 0x22, 0x36, 0x3A, 0x0D, \
	 0xAB, 0xC5, 0x21, 0x97, 0x9B, 0x0D, 0xEA, 0xDA, \
	 0x1D, 0xBF, 0x9A, 0x42, 0xD5, 0xC4, 0x48, 0x4E, \
	 0x0A, 0xBC, 0xD0, 0x6B, 0xFA, 0x53, 0xDD, 0xEF, \
	 0x3C, 0x1B, 0x20, 0xEE, 0x3F, 0xD5, 0x9D, 0x7C, \
	 0x25, 0xE4, 0x1D, 0x2B, 0x66, 0x9E, 0x1E, 0xF1, \
	 0x6E, 0x6F, 0x52, 0xC3, 0x16, 0x4D, 0xF4, 0xFB, \
	 0x79, 0x30, 0xE9, 0xE4, 0xE5, 0x88, 0x57, 0xB6, \
	 0xAC, 0x7D, 0x5F, 0x42, 0xD6, 0x9F, 0x6D, 0x18, \
	 0x77, 0x63, 0xCF, 0x1D, 0x55, 0x03, 0x40, 0x04, \
	 0x87, 0xF5, 0x5B, 0xA5, 0x7E, 0x31, 0xCC, 0x7A, \
	 0x71, 0x35, 0xC8, 0x86, 0xEF, 0xB4, 0x31, 0x8A, \
	 0xED, 0x6A, 0x1E, 0x01, 0x2D, 0x9E, 0x68, 0x32, \
	 0xA9, 0x07, 0x60, 0x0A, 0x91, 0x81, 0x30, 0xC4, \
	 0x6D, 0xC7, 0x78, 0xF9, 0x71, 0xAD, 0x00, 0x38, \
	 0x09, 0x29, 0x99, 0xA3, 0x33, 0xCB, 0x8B, 0x7A, \
	 0x1A, 0x1D, 0xB9, 0x3D, 0x71, 0x40, 0x00, 0x3C, \
	 0x2A, 0x4E, 0xCE, 0xA9, 0xF9, 0x8D, 0x0A, 0xCC, \
	 0x0A, 0x82, 0x91, 0xCD, 0xCE, 0xC9, 0x7D, 0xCF, \
	 0x8E, 0xC9, 0xB5, 0x5A, 0x7F, 0x88, 0xA4, 0x6B, \
	 0x4D, 0xB5, 0xA8, 0x51, 0xF4, 0x41, 0x82, 0xE1, \
	 0xC6, 0x8A, 0x00, 0x7E, 0x5E, 0x0D, 0xD9, 0x02, \
	 0x0B, 0xFD, 0x64, 0xB6, 0x45, 0x03, 0x6C, 0x7A, \
	 0x4E, 0x67, 0x7D, 0x2C, 0x38, 0x53, 0x2A, 0x3A, \
	 0x23, 0xBA, 0x44, 0x42, 0xCA, 0xF5, 0x3E, 0xA6, \
	 0x3B, 0xB4, 0x54, 0x32, 0x9B, 0x76, 0x24, 0xC8, \
	 0x91, 0x7B, 0xDD, 0x64, 0xB1, 0xC0, 0xFD, 0x4C, \
	 0xB3, 0x8E, 0x8C, 0x33, 0x4C, 0x70, 0x1C, 0x3A, \
	 0xCD, 0xAD, 0x06, 0x57, 0xFC, 0xCF, 0xEC, 0x71, \
	 0x9B, 0x1F, 0x5C, 0x3E, 0x4E, 0x46, 0x04, 0x1F, \
	 0x38, 0x81, 0x47, 0xFB, 0x4C, 0xFD, 0xB4, 0x77, \
	 0xA5, 0x24, 0x71, 0xF7, 0xA9, 0xA9, 0x69, 0x10, \
	 0xB8, 0x55, 0x32, 0x2E, 0xDB, 0x63, 0x40, 0xD8, \
	 0xA0, 0x0E, 0xF0, 0x92, 0x35, 0x05, 0x11, 0xE3, \
	 0x0A, 0xBE, 0xC1, 0xFF, 0xF9, 0xE3, 0xA2, 0x6E, \
	 0x7F, 0xB2, 0x9F, 0x8C, 0x18, 0x30, 0x23, 0xC3, \
	 0x58, 0x7E, 0x38, 0xDA, 0x00, 0x77, 0xD9, 0xB4, \
	 0x76, 0x3E, 0x4E, 0x4B, 0x94, 0xB2, 0xBB, 0xC1, \
	 0x94, 0xC6, 0x65, 0x1E, 0x77, 0xCA, 0xF9, 0x92, \
	 0xEE, 0xAA, 0xC0, 0x23, 0x2A, 0x28, 0x1B, 0xF6, \
	 0xB3, 0xA7, 0x39, 0xC1, 0x22, 0x61, 0x16, 0x82, \
	 0x0A, 0xE8, 0xDB, 0x58, 0x47, 0xA6, 0x7C, 0xBE, \
	 0xF9, 0xC9, 0x09, 0x1B, 0x46, 0x2D, 0x53, 0x8C, \
	 0xD7, 0x2B, 0x03, 0x74, 0x6A, 0xE7, 0x7F, 0x5E, \
	 0x62, 0x29, 0x2C, 0x31, 0x15, 0x62, 0xA8, 0x46, \
	 0x50, 0x5D, 0xC8, 0x2D, 0xB8, 0x54, 0x33, 0x8A, \
	 0xE4, 0x9F, 0x52, 0x35, 0xC9, 0x5B, 0x91, 0x17, \
	 0x8C, 0xCF, 0x2D, 0xD5, 0xCA, 0xCE, 0xF4, 0x03, \
	 0xEC, 0x9D, 0x18, 0x10, 0xC6, 0x27, 0x2B, 0x04, \
	 0x5B, 0x3B, 0x71, 0xF9, 0xDC, 0x6B, 0x80, 0xD6, \
	 0x3F, 0xDD, 0x4A, 0x8E, 0x9A, 0xDB, 0x1E, 0x69, \
	 0x62, 0xA6, 0x95, 0x26, 0xD4, 0x31, 0x61, 0xC1, \
	 0xA4, 0x1D, 0x57, 0x0D, 0x79, 0x38, 0xDA, 0xD4, \
	 0xA4, 0x0E, 0x32, 0x9C, 0xD0, 0xE4, 0x0E, 0x65, \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC7919_FFDHE6144_G_BIN { 0x02 }

#define TTLS_DHM_RFC7919_FFDHE8192_P_BIN {		\
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, \
	 0xAD, 0xF8, 0x54, 0x58, 0xA2, 0xBB, 0x4A, 0x9A, \
	 0xAF, 0xDC, 0x56, 0x20, 0x27, 0x3D, 0x3C, 0xF1, \
	 0xD8, 0xB9, 0xC5, 0x83, 0xCE, 0x2D, 0x36, 0x95, \
	 0xA9, 0xE1, 0x36, 0x41, 0x14, 0x64, 0x33, 0xFB, \
	 0xCC, 0x93, 0x9D, 0xCE, 0x24, 0x9B, 0x3E, 0xF9, \
	 0x7D, 0x2F, 0xE3, 0x63, 0x63, 0x0C, 0x75, 0xD8, \
	 0xF6, 0x81, 0xB2, 0x02, 0xAE, 0xC4, 0x61, 0x7A, \
	 0xD3, 0xDF, 0x1E, 0xD5, 0xD5, 0xFD, 0x65, 0x61, \
	 0x24, 0x33, 0xF5, 0x1F, 0x5F, 0x06, 0x6E, 0xD0, \
	 0x85, 0x63, 0x65, 0x55, 0x3D, 0xED, 0x1A, 0xF3, \
	 0xB5, 0x57, 0x13, 0x5E, 0x7F, 0x57, 0xC9, 0x35, \
	 0x98, 0x4F, 0x0C, 0x70, 0xE0, 0xE6, 0x8B, 0x77, \
	 0xE2, 0xA6, 0x89, 0xDA, 0xF3, 0xEF, 0xE8, 0x72, \
	 0x1D, 0xF1, 0x58, 0xA1, 0x36, 0xAD, 0xE7, 0x35, \
	 0x30, 0xAC, 0xCA, 0x4F, 0x48, 0x3A, 0x79, 0x7A, \
	 0xBC, 0x0A, 0xB1, 0x82, 0xB3, 0x24, 0xFB, 0x61, \
	 0xD1, 0x08, 0xA9, 0x4B, 0xB2, 0xC8, 0xE3, 0xFB, \
	 0xB9, 0x6A, 0xDA, 0xB7, 0x60, 0xD7, 0xF4, 0x68, \
	 0x1D, 0x4F, 0x42, 0xA3, 0xDE, 0x39, 0x4D, 0xF4, \
	 0xAE, 0x56, 0xED, 0xE7, 0x63, 0x72, 0xBB, 0x19, \
	 0x0B, 0x07, 0xA7, 0xC8, 0xEE, 0x0A, 0x6D, 0x70, \
	 0x9E, 0x02, 0xFC, 0xE1, 0xCD, 0xF7, 0xE2, 0xEC, \
	 0xC0, 0x34, 0x04, 0xCD, 0x28, 0x34, 0x2F, 0x61, \
	 0x91, 0x72, 0xFE, 0x9C, 0xE9, 0x85, 0x83, 0xFF, \
	 0x8E, 0x4F, 0x12, 0x32, 0xEE, 0xF2, 0x81, 0x83, \
	 0xC3, 0xFE, 0x3B, 0x1B, 0x4C, 0x6F, 0xAD, 0x73, \
	 0x3B, 0xB5, 0xFC, 0xBC, 0x2E, 0xC2, 0x20, 0x05, \
	 0xC5, 0x8E, 0xF1, 0x83, 0x7D, 0x16, 0x83, 0xB2, \
	 0xC6, 0xF3, 0x4A, 0x26, 0xC1, 0xB2, 0xEF, 0xFA, \
	 0x88, 0x6B, 0x42, 0x38, 0x61, 0x1F, 0xCF, 0xDC, \
	 0xDE, 0x35, 0x5B, 0x3B, 0x65, 0x19, 0x03, 0x5B, \
	 0xBC, 0x34, 0xF4, 0xDE, 0xF9, 0x9C, 0x02, 0x38, \
	 0x61, 0xB4, 0x6F, 0xC9, 0xD6, 0xE6, 0xC9, 0x07, \
	 0x7A, 0xD9, 0x1D, 0x26, 0x91, 0xF7, 0xF7, 0xEE, \
	 0x59, 0x8C, 0xB0, 0xFA, 0xC1, 0x86, 0xD9, 0x1C, \
	 0xAE, 0xFE, 0x13, 0x09, 0x85, 0x13, 0x92, 0x70, \
	 0xB4, 0x13, 0x0C, 0x93, 0xBC, 0x43, 0x79, 0x44, \
	 0xF4, 0xFD, 0x44, 0x52, 0xE2, 0xD7, 0x4D, 0xD3, \
	 0x64, 0xF2, 0xE2, 0x1E, 0x71, 0xF5, 0x4B, 0xFF, \
	 0x5C, 0xAE, 0x82, 0xAB, 0x9C, 0x9D, 0xF6, 0x9E, \
	 0xE8, 0x6D, 0x2B, 0xC5, 0x22, 0x36, 0x3A, 0x0D, \
	 0xAB, 0xC5, 0x21, 0x97, 0x9B, 0x0D, 0xEA, 0xDA, \
	 0x1D, 0xBF, 0x9A, 0x42, 0xD5, 0xC4, 0x48, 0x4E, \
	 0x0A, 0xBC, 0xD0, 0x6B, 0xFA, 0x53, 0xDD, 0xEF, \
	 0x3C, 0x1B, 0x20, 0xEE, 0x3F, 0xD5, 0x9D, 0x7C, \
	 0x25, 0xE4, 0x1D, 0x2B, 0x66, 0x9E, 0x1E, 0xF1, \
	 0x6E, 0x6F, 0x52, 0xC3, 0x16, 0x4D, 0xF4, 0xFB, \
	 0x79, 0x30, 0xE9, 0xE4, 0xE5, 0x88, 0x57, 0xB6, \
	 0xAC, 0x7D, 0x5F, 0x42, 0xD6, 0x9F, 0x6D, 0x18, \
	 0x77, 0x63, 0xCF, 0x1D, 0x55, 0x03, 0x40, 0x04, \
	 0x87, 0xF5, 0x5B, 0xA5, 0x7E, 0x31, 0xCC, 0x7A, \
	 0x71, 0x35, 0xC8, 0x86, 0xEF, 0xB4, 0x31, 0x8A, \
	 0xED, 0x6A, 0x1E, 0x01, 0x2D, 0x9E, 0x68, 0x32, \
	 0xA9, 0x07, 0x60, 0x0A, 0x91, 0x81, 0x30, 0xC4, \
	 0x6D, 0xC7, 0x78, 0xF9, 0x71, 0xAD, 0x00, 0x38, \
	 0x09, 0x29, 0x99, 0xA3, 0x33, 0xCB, 0x8B, 0x7A, \
	 0x1A, 0x1D, 0xB9, 0x3D, 0x71, 0x40, 0x00, 0x3C, \
	 0x2A, 0x4E, 0xCE, 0xA9, 0xF9, 0x8D, 0x0A, 0xCC, \
	 0x0A, 0x82, 0x91, 0xCD, 0xCE, 0xC9, 0x7D, 0xCF, \
	 0x8E, 0xC9, 0xB5, 0x5A, 0x7F, 0x88, 0xA4, 0x6B, \
	 0x4D, 0xB5, 0xA8, 0x51, 0xF4, 0x41, 0x82, 0xE1, \
	 0xC6, 0x8A, 0x00, 0x7E, 0x5E, 0x0D, 0xD9, 0x02, \
	 0x0B, 0xFD, 0x64, 0xB6, 0x45, 0x03, 0x6C, 0x7A, \
	 0x4E, 0x67, 0x7D, 0x2C, 0x38, 0x53, 0x2A, 0x3A, \
	 0x23, 0xBA, 0x44, 0x42, 0xCA, 0xF5, 0x3E, 0xA6, \
	 0x3B, 0xB4, 0x54, 0x32, 0x9B, 0x76, 0x24, 0xC8, \
	 0x91, 0x7B, 0xDD, 0x64, 0xB1, 0xC0, 0xFD, 0x4C, \
	 0xB3, 0x8E, 0x8C, 0x33, 0x4C, 0x70, 0x1C, 0x3A, \
	 0xCD, 0xAD, 0x06, 0x57, 0xFC, 0xCF, 0xEC, 0x71, \
	 0x9B, 0x1F, 0x5C, 0x3E, 0x4E, 0x46, 0x04, 0x1F, \
	 0x38, 0x81, 0x47, 0xFB, 0x4C, 0xFD, 0xB4, 0x77, \
	 0xA5, 0x24, 0x71, 0xF7, 0xA9, 0xA9, 0x69, 0x10, \
	 0xB8, 0x55, 0x32, 0x2E, 0xDB, 0x63, 0x40, 0xD8, \
	 0xA0, 0x0E, 0xF0, 0x92, 0x35, 0x05, 0x11, 0xE3, \
	 0x0A, 0xBE, 0xC1, 0xFF, 0xF9, 0xE3, 0xA2, 0x6E, \
	 0x7F, 0xB2, 0x9F, 0x8C, 0x18, 0x30, 0x23, 0xC3, \
	 0x58, 0x7E, 0x38, 0xDA, 0x00, 0x77, 0xD9, 0xB4, \
	 0x76, 0x3E, 0x4E, 0x4B, 0x94, 0xB2, 0xBB, 0xC1, \
	 0x94, 0xC6, 0x65, 0x1E, 0x77, 0xCA, 0xF9, 0x92, \
	 0xEE, 0xAA, 0xC0, 0x23, 0x2A, 0x28, 0x1B, 0xF6, \
	 0xB3, 0xA7, 0x39, 0xC1, 0x22, 0x61, 0x16, 0x82, \
	 0x0A, 0xE8, 0xDB, 0x58, 0x47, 0xA6, 0x7C, 0xBE, \
	 0xF9, 0xC9, 0x09, 0x1B, 0x46, 0x2D, 0x53, 0x8C, \
	 0xD7, 0x2B, 0x03, 0x74, 0x6A, 0xE7, 0x7F, 0x5E, \
	 0x62, 0x29, 0x2C, 0x31, 0x15, 0x62, 0xA8, 0x46, \
	 0x50, 0x5D, 0xC8, 0x2D, 0xB8, 0x54, 0x33, 0x8A, \
	 0xE4, 0x9F, 0x52, 0x35, 0xC9, 0x5B, 0x91, 0x17, \
	 0x8C, 0xCF, 0x2D, 0xD5, 0xCA, 0xCE, 0xF4, 0x03, \
	 0xEC, 0x9D, 0x18, 0x10, 0xC6, 0x27, 0x2B, 0x04, \
	 0x5B, 0x3B, 0x71, 0xF9, 0xDC, 0x6B, 0x80, 0xD6, \
	 0x3F, 0xDD, 0x4A, 0x8E, 0x9A, 0xDB, 0x1E, 0x69, \
	 0x62, 0xA6, 0x95, 0x26, 0xD4, 0x31, 0x61, 0xC1, \
	 0xA4, 0x1D, 0x57, 0x0D, 0x79, 0x38, 0xDA, 0xD4, \
	 0xA4, 0x0E, 0x32, 0x9C, 0xCF, 0xF4, 0x6A, 0xAA, \
	 0x36, 0xAD, 0x00, 0x4C, 0xF6, 0x00, 0xC8, 0x38, \
	 0x1E, 0x42, 0x5A, 0x31, 0xD9, 0x51, 0xAE, 0x64, \
	 0xFD, 0xB2, 0x3F, 0xCE, 0xC9, 0x50, 0x9D, 0x43, \
	 0x68, 0x7F, 0xEB, 0x69, 0xED, 0xD1, 0xCC, 0x5E, \
	 0x0B, 0x8C, 0xC3, 0xBD, 0xF6, 0x4B, 0x10, 0xEF, \
	 0x86, 0xB6, 0x31, 0x42, 0xA3, 0xAB, 0x88, 0x29, \
	 0x55, 0x5B, 0x2F, 0x74, 0x7C, 0x93, 0x26, 0x65, \
	 0xCB, 0x2C, 0x0F, 0x1C, 0xC0, 0x1B, 0xD7, 0x02, \
	 0x29, 0x38, 0x88, 0x39, 0xD2, 0xAF, 0x05, 0xE4, \
	 0x54, 0x50, 0x4A, 0xC7, 0x8B, 0x75, 0x82, 0x82, \
	 0x28, 0x46, 0xC0, 0xBA, 0x35, 0xC3, 0x5F, 0x5C, \
	 0x59, 0x16, 0x0C, 0xC0, 0x46, 0xFD, 0x82, 0x51, \
	 0x54, 0x1F, 0xC6, 0x8C, 0x9C, 0x86, 0xB0, 0x22, \
	 0xBB, 0x70, 0x99, 0x87, 0x6A, 0x46, 0x0E, 0x74, \
	 0x51, 0xA8, 0xA9, 0x31, 0x09, 0x70, 0x3F, 0xEE, \
	 0x1C, 0x21, 0x7E, 0x6C, 0x38, 0x26, 0xE5, 0x2C, \
	 0x51, 0xAA, 0x69, 0x1E, 0x0E, 0x42, 0x3C, 0xFC, \
	 0x99, 0xE9, 0xE3, 0x16, 0x50, 0xC1, 0x21, 0x7B, \
	 0x62, 0x48, 0x16, 0xCD, 0xAD, 0x9A, 0x95, 0xF9, \
	 0xD5, 0xB8, 0x01, 0x94, 0x88, 0xD9, 0xC0, 0xA0, \
	 0xA1, 0xFE, 0x30, 0x75, 0xA5, 0x77, 0xE2, 0x31, \
	 0x83, 0xF8, 0x1D, 0x4A, 0x3F, 0x2F, 0xA4, 0x57, \
	 0x1E, 0xFC, 0x8C, 0xE0, 0xBA, 0x8A, 0x4F, 0xE8, \
	 0xB6, 0x85, 0x5D, 0xFE, 0x72, 0xB0, 0xA6, 0x6E, \
	 0xDE, 0xD2, 0xFB, 0xAB, 0xFB, 0xE5, 0x8A, 0x30, \
	 0xFA, 0xFA, 0xBE, 0x1C, 0x5D, 0x71, 0xA8, 0x7E, \
	 0x2F, 0x74, 0x1E, 0xF8, 0xC1, 0xFE, 0x86, 0xFE, \
	 0xA6, 0xBB, 0xFD, 0xE5, 0x30, 0x67, 0x7F, 0x0D, \
	 0x97, 0xD1, 0x1D, 0x49, 0xF7, 0xA8, 0x44, 0x3D, \
	 0x08, 0x22, 0xE5, 0x06, 0xA9, 0xF4, 0x61, 0x4E, \
	 0x01, 0x1E, 0x2A, 0x94, 0x83, 0x8F, 0xF8, 0x8C, \
	 0xD6, 0x8C, 0x8B, 0xB7, 0xC5, 0xC6, 0x42, 0x4C, \
	 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }

#define TTLS_DHM_RFC7919_FFDHE8192_G_BIN { 0x02 }

#endif /* dhm.h */
