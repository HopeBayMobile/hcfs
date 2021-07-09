/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OPENSSL_HEADER_OBJECTS_H
#define OPENSSL_HEADER_OBJECTS_H

#include <openssl/base.h>

#include <openssl/bytestring.h>
#include <openssl/obj_mac.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* The objects library deals with the registration and indexing of ASN.1 object
 * identifiers. These values are often written as a dotted sequence of numbers,
 * e.g. 1.2.840.113549.1.9.16.3.9.
 *
 * Internally, OpenSSL likes to deal with these values by numbering them with
 * numbers called "nids". OpenSSL has a large, built-in database of common
 * object identifiers and also has both short and long names for them.
 *
 * This library provides functions for translating between object identifiers,
 * nids, short names and long names.
 *
 * The nid values should not be used outside of a single process: they are not
 * stable identifiers. */


/* Basic operations. */

/* OBJ_dup returns a duplicate copy of |obj| or NULL on allocation failure. */
OPENSSL_EXPORT ASN1_OBJECT *OBJ_dup(const ASN1_OBJECT *obj);

/* OBJ_cmp returns a value less than, equal to or greater than zero if |a| is
 * less than, equal to or greater than |b|, respectively. */
OPENSSL_EXPORT int OBJ_cmp(const ASN1_OBJECT *a, const ASN1_OBJECT *b);


/* Looking up nids. */

/* OBJ_obj2nid returns the nid corresponding to |obj|, or |NID_undef| if no
 * such object is known. */
OPENSSL_EXPORT int OBJ_obj2nid(const ASN1_OBJECT *obj);

/* OBJ_cbs2nid returns the nid corresponding to the DER data in |cbs|, or
 * |NID_undef| if no such object is known. */
OPENSSL_EXPORT int OBJ_cbs2nid(const CBS *cbs);

/* OBJ_sn2nid returns the nid corresponding to |short_name|, or |NID_undef| if
 * no such short name is known. */
OPENSSL_EXPORT int OBJ_sn2nid(const char *short_name);

/* OBJ_ln2nid returns the nid corresponding to |long_name|, or |NID_undef| if
 * no such long name is known. */
OPENSSL_EXPORT int OBJ_ln2nid(const char *long_name);

/* OBJ_txt2nid returns the nid corresponding to |s|, which may be a short name,
 * long name, or an ASCII string containing a dotted sequence of numbers. It
 * returns the nid or NID_undef if unknown. */
OPENSSL_EXPORT int OBJ_txt2nid(const char *s);


/* Getting information about nids. */

/* OBJ_nid2obj returns the ASN1_OBJECT corresponding to |nid|, or NULL if |nid|
 * is unknown. */
OPENSSL_EXPORT const ASN1_OBJECT *OBJ_nid2obj(int nid);

/* OBJ_nid2sn returns the short name for |nid|, or NULL if |nid| is unknown. */
OPENSSL_EXPORT const char *OBJ_nid2sn(int nid);

/* OBJ_nid2sn returns the long name for |nid|, or NULL if |nid| is unknown. */
OPENSSL_EXPORT const char *OBJ_nid2ln(int nid);

/* OBJ_nid2cbs writes |nid| as an ASN.1 OBJECT IDENTIFIER to |out|. It returns
 * one on success or zero otherwise. */
OPENSSL_EXPORT int OBJ_nid2cbb(CBB *out, int nid);


/* Dealing with textual representations of object identifiers. */

/* OBJ_txt2obj returns an ASN1_OBJECT for the textual respresentation in |s|.
 * If |dont_search_names| is zero, then |s| will be matched against the long
 * and short names of a known objects to find a match. Otherwise |s| must
 * contain an ASCII string with a dotted sequence of numbers. The resulting
 * object need not be previously known. It returns a freshly allocated
 * |ASN1_OBJECT| or NULL on error. */
OPENSSL_EXPORT ASN1_OBJECT *OBJ_txt2obj(const char *s, int dont_search_names);

/* OBJ_obj2txt converts |obj| to a textual representation. If
 * |dont_return_name| is zero then |obj| will be matched against known objects
 * and the long (preferably) or short name will be used if found. Otherwise
 * |obj| will be converted into a dotted sequence of integers. If |out| is not
 * NULL, then at most |out_len| bytes of the textual form will be written
 * there. If |out_len| is at least one, then string written to |out| will
 * always be NUL terminated. It returns the number of characters that could
 * have been written, not including the final NUL, or -1 on error. */
OPENSSL_EXPORT int OBJ_obj2txt(char *out, int out_len, const ASN1_OBJECT *obj,
                               int dont_return_name);


/* Adding objects at runtime. */

/* OBJ_create adds a known object and returns the nid of the new object, or
 * NID_undef on error. */
OPENSSL_EXPORT int OBJ_create(const char *oid, const char *short_name,
                              const char *long_name);


/* Handling signature algorithm identifiers.
 *
 * Some NIDs (e.g. sha256WithRSAEncryption) specify both a digest algorithm and
 * a public key algorithm. The following functions map between pairs of digest
 * and public-key algorithms and the NIDs that specify their combination.
 *
 * Sometimes the combination NID leaves the digest unspecified (e.g.
 * rsassaPss). In these cases, the digest NID is |NID_undef|. */

/* OBJ_find_sigid_algs finds the digest and public-key NIDs that correspond to
 * the signing algorithm |sign_nid|. If successful, it sets |*out_digest_nid|
 * and |*out_pkey_nid| and returns one. Otherwise it returns zero. Any of
 * |out_digest_nid| or |out_pkey_nid| can be NULL if the caller doesn't need
 * that output value. */
OPENSSL_EXPORT int OBJ_find_sigid_algs(int sign_nid, int *out_digest_nid,
                                       int *out_pkey_nid);

/* OBJ_find_sigid_by_algs finds the signature NID that corresponds to the
 * combination of |digest_nid| and |pkey_nid|. If success, it sets
 * |*out_sign_nid| and returns one. Otherwise it returns zero. The
 * |out_sign_nid| argument can be NULL if the caller only wishes to learn
 * whether the combination is valid. */
OPENSSL_EXPORT int OBJ_find_sigid_by_algs(int *out_sign_nid, int digest_nid,
                                          int pkey_nid);


#if defined(__cplusplus)
}  /* extern C */
#endif

#define OBJ_F_OBJ_create 100
#define OBJ_F_OBJ_dup 101
#define OBJ_F_OBJ_nid2obj 102
#define OBJ_F_OBJ_txt2obj 103
#define OBJ_R_UNKNOWN_NID 100

#endif  /* OPENSSL_HEADER_OBJECTS_H */
