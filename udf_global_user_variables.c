/* $Id$ */
/*
 * Copyright (c) 2007-2009 Frank DENIS <j@pureftpd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <mysql.h>
#include <pthread.h>

#define MAX_NAME_LENGTH  256
#define MAX_VALUE_LENGTH 65536

typedef struct Global_ {
    size_t name_len;
    struct Global_ *next;    
    char *name;
    size_t value_len;    
    char *value;
} Global;

static pthread_mutex_t bglmtx = PTHREAD_MUTEX_INITIALIZER;
static Global *globals_list_first;

static Global *find_global(const char * const name, const size_t name_len)
{
    Global *scanned_node = globals_list_first;
    
    if (scanned_node == NULL) {
        return NULL;
    }
    do {
        if (scanned_node->name_len == name_len &&
            memcmp(scanned_node->name, name, name_len) == 0) {
            return scanned_node;
        }
        scanned_node = scanned_node->next;
    } while (scanned_node != NULL);
    
    return NULL;
}

static int store_global(const char * const name, const size_t name_len,
                        const char * const value, const size_t value_len)
{
    Global *global = NULL;
    char *dup_value;
    
    if ((dup_value = malloc(value_len)) == NULL) {
        return -1;
    }
    memcpy(dup_value, value, value_len);    
    global = find_global(name, name_len);
    if (global != NULL) {
        free(global->value);
        global->value = dup_value;
        global->value_len = value_len;
        return 0;
    }
    if ((global = malloc(sizeof *global)) == NULL) {
        goto err;
    }
    if ((global->name = malloc(name_len)) == NULL) {
        err:
        free(global);
        free(dup_value);
        return -1;
    }
    global->name_len = name_len;
    global->next = NULL;    
    memcpy(global->name, name, name_len);
    global->value_len = value_len;    
    global->value = dup_value;
    if (globals_list_first == NULL) {
        globals_list_first = global;
    } else {
        global->next = globals_list_first;
        globals_list_first = global;
    }    
    return 0;
}

my_bool global_set_init(UDF_INIT * const initid, UDF_ARGS * args,
                        char * const message)
{
    if (args->arg_count != 2) {
        snprintf(message, MYSQL_ERRMSG_SIZE,
                 "Usage: global_set(<variable name>, <value>)");
        return 1;
    }
    if (args->lengths[0] > MAX_NAME_LENGTH ||
        args->lengths[1] > MAX_VALUE_LENGTH) {
        snprintf(message, MYSQL_ERRMSG_SIZE,
                 "name: %lu bytes max - value: %lu bytes max",
                 (unsigned long) MAX_NAME_LENGTH,
                 (unsigned long) MAX_VALUE_LENGTH);
        return 1;
    }
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    
    initid->maybe_null = 0;
    initid->max_length = 1;
    initid->const_item = 1;
    initid->ptr = NULL;
    
    return 0;
}

my_bool global_store_init(UDF_INIT * const initid, UDF_ARGS * args,
                        char * const message)
{
    return global_set_init(initid, args, message);
}

long long global_set(UDF_INIT * const initid, UDF_ARGS * const args,
                     char * const is_null, char * const error)
{
    (void) initid;

    *is_null = 0;
    *error = 0;

    if (pthread_mutex_lock(&bglmtx) != 0) {
        *error = 1;
        return 0LL;
    }
    if (store_global(args->args[0], args->lengths[0],
                     args->args[1], args->lengths[1]) != 0) {
        pthread_mutex_unlock(&bglmtx);        
        *error = 1;
        return 0LL;
    }
    if (pthread_mutex_unlock(&bglmtx) != 0) {
        *error = 1;
        return 0LL;
    }
    return 1LL;
}

long long global_store(UDF_INIT * const initid, UDF_ARGS * const args,
		       char * const is_null, char * const error)
{
    return global_set(initid, args, is_null, error);
}

my_bool global_get_init(UDF_INIT * const initid, UDF_ARGS * args,
                        char * const message)
{
    if (args->arg_count != 1) {
        snprintf(message, MYSQL_ERRMSG_SIZE,
                 "Usage: global_get(<variable name>)");
        return 1;
    }
    if (args->lengths[0] > MAX_NAME_LENGTH) {
        snprintf(message, MYSQL_ERRMSG_SIZE,
                 "name: %lu bytes max", (unsigned long) MAX_NAME_LENGTH);
        return 1;
    }
    args->arg_type[0] = STRING_RESULT;    
    initid->maybe_null = 1;
    initid->max_length = MAX_VALUE_LENGTH;
    initid->const_item = 0;
    initid->ptr = NULL;
    
    return 0;
}

char *global_get(UDF_INIT * const initid, UDF_ARGS * const args,
                 char * const result, unsigned long * const length,
                 char * const is_null, char * const error)
{
    char *ret = result;
    Global *global;

    (void) initid;
    
    *is_null = 0;
    *error = 0;
    *result = 0;
    *length = 0UL;    
    if (pthread_mutex_lock(&bglmtx) != 0) {
        *error = 1;
        return ret;
    }
    if ((global = find_global(args->args[0], args->lengths[0])) == NULL) {
        *is_null = 1;
        goto xret;
    }
    if (global->value_len < 256) {
        memcpy(result, global->value, global->value_len);
        *length = (unsigned long) global->value_len;
        goto xret;
    }
    if ((ret = malloc(global->value_len)) == NULL) {
        ret = result;
        *error = 1;
        goto xret;
    }
    *length = (unsigned long) global->value_len;    
    memcpy(ret, global->value, global->value_len);
    xret:
    if (pthread_mutex_unlock(&bglmtx) != 0) {
        *error = 1;
    }
    return ret;
}

static my_bool global_add_init_common(UDF_INIT * const initid, UDF_ARGS * args,
                                      char * const message, const int variant)
{
    if (args->arg_count != 2) {
        const char *s;
        
        if (variant == 0) {
            s = "Usage: global_add(<variable name>, <value to add>)";
        } else {
            s = "Usage: global_addp(<variable name>, <value to add>)";
        }
        snprintf(message, MYSQL_ERRMSG_SIZE, s);

        return 1;
    }
    if (args->lengths[0] > MAX_NAME_LENGTH) {
        snprintf(message, MYSQL_ERRMSG_SIZE,
                 "name: %lu bytes max", (unsigned long) MAX_NAME_LENGTH);
        return 1;
    }
    args->arg_type[0] = STRING_RESULT;
    if (args->arg_type[1] != INT_RESULT) {
        snprintf(message, MYSQL_ERRMSG_SIZE, "the value must be an integer");
        return 1;        
    }
    initid->maybe_null = 1;
    initid->max_length = MAX_VALUE_LENGTH;
    initid->const_item = 0;
    initid->ptr = NULL;
    
    return 0;
}

static long long global_add_common(UDF_INIT * const initid,
                                   UDF_ARGS * const args, char * const is_null,
                                   char * const error, const int variant)
{
    Global *global;
    long long nvalue = 0LL, onvalue = 0LL;
    char *snvalue;

    (void) initid;
    
    *is_null = 0;
    *error = 0;
    if (pthread_mutex_lock(&bglmtx) != 0) {
        *error = 1;
        return 0;
    }
    if ((global = find_global(args->args[0], args->lengths[0])) == NULL) {
        *is_null = 1;
        goto xret;
    }
    nvalue = onvalue = strtoll(global->value, NULL, 10);
    nvalue += * ((long long *) args->args[1]);
    asprintf(&snvalue, "%lld", nvalue);
    if (snvalue == NULL) {
        *error = 1;
        goto xret;
    }
    global->value_len = strlen(snvalue);    
    global->value = snvalue;
    xret:
    if (pthread_mutex_unlock(&bglmtx) != 0) {
        *error = 1;
    }
    if (variant == 0) {
        return nvalue;
    }
    return onvalue;
}

my_bool global_add_init(UDF_INIT * const initid, UDF_ARGS * args,
                        char * const message)
{
    return global_add_init_common(initid, args, message, 0);
}

long long global_add(UDF_INIT * const initid, UDF_ARGS * const args,
                     char * const is_null, char * const error)
{
    return global_add_common(initid, args, is_null, error, 0);
}

my_bool global_addp_init(UDF_INIT * const initid, UDF_ARGS * args,
                         char * const message)
{
    return global_add_init_common(initid, args, message, 1);
}

long long global_addp(UDF_INIT * const initid, UDF_ARGS * const args,
                      char * const is_null, char * const error)
{
    return global_add_common(initid, args, is_null, error, 1);
}
