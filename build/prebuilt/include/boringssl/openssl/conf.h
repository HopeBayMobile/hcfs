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

#ifndef OPENSSL_HEADER_CONF_H
#define OPENSSL_HEADER_CONF_H

#include <openssl/base.h>

#include <openssl/stack.h>
#include <openssl/lhash.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Config files look like:
 *
 *   # Comment
 *
 *   # This key is in the default section.
 *   key=value
 *
 *   [section_name]
 *   key2=value2
 *
 * Config files are representated by a |CONF|. */

struct conf_value_st {
  char *section;
  char *name;
  char *value;
};

struct conf_st {
  LHASH_OF(CONF_VALUE) *data;
};


/* NCONF_new returns a fresh, empty |CONF|, or NULL on error. The |method|
 * argument must be NULL. */
CONF *NCONF_new(void *method);

/* NCONF_free frees all the data owned by |conf| and then |conf| itself. */
void NCONF_free(CONF *conf);

/* NCONF_load parses the file named |filename| and adds the values found to
 * |conf|. It returns one on success and zero on error. In the event of an
 * error, if |out_error_line| is not NULL, |*out_error_line| is set to the
 * number of the line that contained the error. */
int NCONF_load(CONF *conf, const char *filename, long *out_error_line);

/* NCONF_load_bio acts like |NCONF_load| but reads from |bio| rather than from
 * a named file. */
int NCONF_load_bio(CONF *conf, BIO *bio, long *out_error_line);

/* NCONF_get_section returns a stack of values for a given section in |conf|.
 * If |section| is NULL, the default section is returned. It returns NULL on
 * error. */
STACK_OF(CONF_VALUE) *NCONF_get_section(const CONF *conf, const char *section);

/* NCONF_get_string returns the value of the key |name|, in section |section|.
 * The |section| argument may be NULL to indicate the default section. It
 * returns the value or NULL on error. */
const char *NCONF_get_string(const CONF *conf, const char *section,
                             const char *name);


/* Utility functions */

/* CONF_parse_list takes a list separated by 'sep' and calls |list_cb| giving
 * the start and length of each member, optionally stripping leading and
 * trailing whitespace. This can be used to parse comma separated lists for
 * example. If |list_cb| returns <= 0, then the iteration is halted and that
 * value is returned immediately. Otherwise it returns one. Note that |list_cb|
 * may be called on an empty member. */
int CONF_parse_list(const char *list, char sep, int remove_whitespace,
                    int (*list_cb)(const char *elem, int len, void *usr),
                    void *arg);

#if defined(__cplusplus)
}  /* extern C */
#endif

#define CONF_F_CONF_parse_list 100
#define CONF_F_NCONF_load 101
#define CONF_F_def_load_bio 102
#define CONF_F_str_copy 103
#define CONF_R_LIST_CANNOT_BE_NULL 100
#define CONF_R_MISSING_CLOSE_SQUARE_BRACKET 101
#define CONF_R_MISSING_EQUAL_SIGN 102
#define CONF_R_NO_CLOSE_BRACE 103
#define CONF_R_UNABLE_TO_CREATE_NEW_SECTION 104
#define CONF_R_VARIABLE_HAS_NO_VALUE 105

#endif  /* OPENSSL_HEADER_THREAD_H */
