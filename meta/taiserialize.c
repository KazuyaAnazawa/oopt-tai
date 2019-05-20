/**
 * @file    taiserialize.c
 *
 * @brief   This file implements basic serialization functions for
 *          TAI attributes
 *
 * @copyright Copyright (c) 2014 Microsoft Open Technologies, Inc.
 * @copyright Copyright (c) 2018 Nippon Telegraph and Telephone Corporation
 *
 * @remark  Licensed under the Apache License, Version 2.0 (the "License"); you
 *          may not use this file except in compliance with the License. You may
 *          obtain a copy of the License at
 *          http://www.apache.org/licenses/LICENSE-2.0
 *
 * @remark  THIS CODE IS PROVIDED ON AN *AS IS* BASIS, WITHOUT WARRANTIES OR
 *          CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT
 *          LIMITATION ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS
 *          FOR A PARTICULAR PURPOSE, MERCHANTABILITY OR NON-INFRINGEMENT.
 *
 * @remark  See the Apache Version 2.0 License for specific language governing
 *          permissions and limitations under the License.
 *
 * @remark  Microsoft would like to thank the following companies for their
 *          review and assistance with these files: Intel Corporation, Mellanox
 *          Technologies Ltd, Dell Products, L.P., Facebook, Inc., Marvell
 *          International Ltd.
 */

#include <arpa/inet.h>
#include <byteswap.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tai.h>
#include "taimetadatautils.h"
#include "taimetadatalogger.h"
#include "taiserialize.h"

#define PRIMITIVE_BUFFER_SIZE 128
#define MAX_CHARS_PRINT 25

bool tai_serialize_is_char_allowed(
        _In_ char c)
{
    /*
     * When we will perform deserialize, we allow buffer string to be
     * terminated not only by zero, but also with json characters like:
     *
     * - end of quote
     * - comma, next item in array
     * - end of array
     *
     * This will be handy when performing deserialize.
     */

    return c == 0 || c == '"' || c == ',' || c == ']' || c == '}';
}

int tai_serialize_bool(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ bool flag)
{
    return snprintf(buffer, n, "%s", flag ? "true" : "false");
}

#define TAI_TRUE_LENGTH 4
#define TAI_FALSE_LENGTH 5

int tai_deserialize_bool(
        _In_ const char *buffer,
        _Out_ bool *flag)
{
    if (strncmp(buffer, "true", TAI_TRUE_LENGTH) == 0 &&
            tai_serialize_is_char_allowed(buffer[TAI_TRUE_LENGTH]))
    {
        *flag = true;
        return TAI_TRUE_LENGTH;
    }

    if (strncmp(buffer, "false", TAI_FALSE_LENGTH) == 0 &&
            tai_serialize_is_char_allowed(buffer[TAI_FALSE_LENGTH]))
    {
        *flag = false;
        return TAI_FALSE_LENGTH;
    }

    /*
     * Limit printf to maximum "false" length + 1 if there is invalid character
     * after "false" string.
     */

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as bool",
            TAI_FALSE_LENGTH + 1,
            buffer);

    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_chardata(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ const char data[TAI_CHARDATA_LENGTH])
{
    int idx;

    if ( n < TAI_CHARDATA_LENGTH ) {
        return TAI_CHARDATA_LENGTH;
    }

    for (idx = 0; idx < TAI_CHARDATA_LENGTH; ++idx)
    {
        char c = data[idx];

        if (c == 0)
        {
            break;
        }

        if (isprint(c) && c != '\\' && c != '"')
        {
            buffer[idx] = c;
            continue;
        }

        TAI_META_LOG_WARN("invalid character 0x%x in chardata", c);
        return TAI_SERIALIZE_ERROR;
    }

    buffer[idx] = 0;

    return idx;
}

int tai_deserialize_chardata(
        _In_ const char *buffer,
        _Out_ char data[TAI_CHARDATA_LENGTH])
{
    int idx;

    memset(data, 0, TAI_CHARDATA_LENGTH);

    for (idx = 0; idx < TAI_CHARDATA_LENGTH; ++idx)
    {
        char c = buffer[idx];

        if (isprint(c) && c != '\\' && c != '"')
        {
            data[idx] = c;
            continue;
        }

        if (c == 0)
        {
            break;
        }

        if (c == '"')
        {
            /*
             * We allow quote as last char since chardata will be serialized in
             * quotes.
             */

            break;
        }

        TAI_META_LOG_WARN("invalid character 0x%x in chardata", c);
        return TAI_SERIALIZE_ERROR;
    }

    if (tai_serialize_is_char_allowed(buffer[idx]))
    {
        return idx;
    }

    TAI_META_LOG_WARN("invalid character 0x%x in chardata", buffer[idx]);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_uint8(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ uint8_t u8)
{
    return snprintf(buffer, n, "%u", u8);
}

int tai_deserialize_uint8(
        _In_ const char *buffer,
        _Out_ uint8_t *u8)
{
    uint64_t u64;

    int res = tai_deserialize_uint64(buffer, &u64);

    if (res > 0 && u64 <= UCHAR_MAX)
    {
        *u8 = (uint8_t)u64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as uint8", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_int8(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ int8_t u8)
{
    return snprintf(buffer, n, "%d", u8);
}

int tai_deserialize_int8(
        _In_ const char *buffer,
        _Out_ int8_t *s8)
{
    int64_t s64;

    int res = tai_deserialize_int64(buffer, &s64);

    if (res > 0 && s64 >= CHAR_MIN && s64 <= CHAR_MAX)
    {
        *s8 = (int8_t)s64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as int8", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_uint16(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ uint16_t u16)
{
    return snprintf(buffer, n, "%u", u16);
}

int tai_deserialize_uint16(
        _In_ const char *buffer,
        _Out_ uint16_t *u16)
{
    uint64_t u64;

    int res = tai_deserialize_uint64(buffer, &u64);

    if (res > 0 && u64 <= USHRT_MAX)
    {
        *u16 = (uint16_t)u64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as uint16", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_int16(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ int16_t s16)
{
    return snprintf(buffer, n, "%d", s16);
}

int tai_deserialize_int16(
        _In_ const char *buffer,
        _Out_ int16_t *s16)
{
    int64_t s64;

    int res = tai_deserialize_int64(buffer, &s64);

    if (res > 0 && s64 >= SHRT_MIN && s64 <= SHRT_MAX)
    {
        *s16 = (int16_t)s64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as int16", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_uint32(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ uint32_t u32)
{
    return snprintf(buffer, n, "%u", u32);
}

int tai_deserialize_uint32(
        _In_ const char *buffer,
        _Out_ uint32_t *u32)
{
    uint64_t u64;

    int res = tai_deserialize_uint64(buffer, &u64);

    if (res > 0 && u64 <= UINT_MAX)
    {
        *u32 = (uint32_t)u64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as uint32", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_int32(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ int32_t s32)
{
    return snprintf(buffer, n, "%d", s32);
}

int tai_deserialize_int32(
        _In_ const char *buffer,
        _Out_ int32_t *s32)
{
    int64_t s64;

    int res = tai_deserialize_int64(buffer, &s64);

    if (res > 0 && s64 >= INT_MIN && s64 <= INT_MAX)
    {
        *s32 = (int32_t)s64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as int32", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_uint64(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ uint64_t u64)
{
    return snprintf(buffer, n, "%lu", u64);
}

#define TAI_BASE_10 10

int tai_deserialize_uint64(
        _In_ const char *buffer,
        _Out_ uint64_t *u64)
{
    int idx = 0;
    uint64_t result = 0;

    while (isdigit(buffer[idx]))
    {
        char c = (char)(buffer[idx] - '0');

        /*
         * Base is 10 we can check, that if result is greater than (2^64-1)/10)
         * then next multiplication with 10 will cause overflow.
         */

        if (result > (ULONG_MAX/TAI_BASE_10) ||
            ((result == ULONG_MAX/TAI_BASE_10) && (c > (char)(ULONG_MAX % TAI_BASE_10))))
        {
            idx = 0;
            break;
        }

        result = result * 10 + (uint64_t)(c);

        idx++;
    }

    if (idx > 0 && tai_serialize_is_char_allowed(buffer[idx]))
    {
        *u64 = result;
        return idx;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s...' as uint64", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_int64(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ int64_t s64)
{
    return snprintf(buffer, n, "%ld", s64);
}

int tai_deserialize_int64(
        _In_ const char *buffer,
        _Out_ int64_t *s64)
{
    uint64_t result = 0;
    bool negative = 0;

    if (*buffer == '-')
    {
        buffer++;
        negative = true;
    }

    int res = tai_deserialize_uint64(buffer, &result);

    if (res > 0)
    {
        if (negative)
        {
            if (result <= (uint64_t)(LONG_MIN))
            {
                *s64 = -(int64_t)result;
                return res + 1;
            }
        }
        else
        {
            if (result <= LONG_MAX)
            {
                *s64 = (int64_t)result;
                return res;
            }
        }
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as int64", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_deserialize_u32range(
        _In_ const char *buffer,
        _Out_ tai_u32_range_t *value)
{
    int ret = tai_deserialize_uint32(buffer, &value->min), ret2;
    if ( ret < 0 ) {
        return ret;
    }
    if ( buffer[ret++] != ',' ) {
        return -1;
    }
    ret2 = tai_deserialize_uint32(buffer+ret, &value->max);
    return ret + ret2;
}

int tai_deserialize_s32range(
        _In_ const char *buffer,
        _Out_ tai_s32_range_t *value)
{
    int ret = tai_deserialize_int32(buffer, &value->min), ret2;
    if ( ret < 0 ) {
        return ret;
    }
    if ( buffer[ret++] != ',' ) {
        return -1;
    }
    ret2 = tai_deserialize_int32(buffer+ret, &value->max);
    return ret + ret2;
}

int tai_deserialize_charlist(
        _In_ const char *buffer,
        _Out_ tai_char_list_t *value)
{
    int count = strlen(buffer) + 1;
    if ( count > value->count ) {
        return -1;
    }
    memcpy(value->list, buffer, count);
    return count;
}

int tai_serialize_float(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ float flt)
{
    return snprintf(buffer, n, "%f", flt);
}

int tai_deserialize_float(
        _In_ const char *buffer,
        _Out_ float *flt)
{
    int idx = 0;
    while(true)
    {
        if ( tai_serialize_is_char_allowed(buffer[idx++]) ) {
            break;
        }
    }
    double v = atof(buffer);
    *flt = (float)v;
    return idx - 1;
}

int tai_serialize_size(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ tai_size_t size)
{
    return snprintf(buffer, n, "%zu", size);
}

int tai_deserialize_size(
        _In_ const char *buffer,
        _Out_ tai_size_t *size)
{
    uint64_t u64;

    int res = tai_deserialize_uint64(buffer, &u64);

    if (res > 0)
    {
        *size = (tai_size_t)u64;
        return res;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s...' as tai_size_t", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_object_id(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ tai_object_id_t oid)
{
    return snprintf(buffer, n, "oid:0x%lx", oid);
}

int tai_deserialize_object_id(
        _In_ const char *buffer,
        _Out_ tai_object_id_t *oid)
{
    int read;

    int n = sscanf(buffer, "oid:0x%16lx%n", oid, &read);

    if (n == 1 && tai_serialize_is_char_allowed(buffer[read]))
    {
        return read;
    }

    TAI_META_LOG_WARN("failed to deserialize '%.*s' as oid", MAX_CHARS_PRINT, buffer);
    return TAI_SERIALIZE_ERROR;
}

int tai_serialize_enum(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ const tai_enum_metadata_t* meta,
        _In_ int32_t value,
        _In_ const tai_serialize_option_t *option)
{
    if (meta == NULL)
    {
        return tai_serialize_int32(buffer, n, value);
    }
    bool short_ = false;
    if ( option != NULL && option->human ) {
        short_ = true;
    }

    char *fmt = "%s";
    if ( option != NULL && option->json ) {
        fmt = "\"%s\"";
    }

    size_t i = 0;

    for (; i < meta->valuescount; ++i)
    {
        if (meta->values[i] == value)
        {
            if ( short_ ) {
                return snprintf(buffer, n, fmt, meta->valuesshortnames[i]);
            } else {
                return snprintf(buffer, n, fmt, meta->valuesnames[i]);
            }
        }
    }

    TAI_META_LOG_WARN("enum value %d not found in enum %s", value, meta->name);

    return tai_serialize_int32(buffer, n, value);
}

int tai_deserialize_enum(
        _In_ const char *buffer,
        _In_ const tai_enum_metadata_t* meta,
        _Out_ int32_t *value,
        _In_ const tai_serialize_option_t *option)
{
    if (meta == NULL)
    {
        return tai_deserialize_int32(buffer, value);
    }

    const char* const* names = meta->valuesnames;
    if ( option != NULL && option->human ) {
        names = meta->valuesshortnames;
    }

    size_t idx = 0;

    for (; idx < meta->valuescount; ++idx)
    {
        size_t len = strlen(names[idx]);

        if (strncmp(names[idx], buffer, len) == 0 &&
            tai_serialize_is_char_allowed(buffer[len]))
        {
            *value = meta->values[idx];
            return (int)len;
        }
    }

    TAI_META_LOG_WARN("enum value '%.*s' not found in enum %s", MAX_CHARS_PRINT, buffer, meta->name);

    return tai_deserialize_int32(buffer, value);
}

#define _SERIALIZE(sentence, count, ptr, n) \
    count = sentence;\
    if ( count < 0 || count > n ) {\
        return (ptr - buffer) + count;\
    }\
    ptr += count;\
    n -= count;\

#define _TAI_SERIALIZE_LIST(field1, field2) \
    if ( option->json ) { \
        _SERIALIZE(snprintf(ptr, n, "["), count, ptr, n);\
    } \
    for ( i = 0; i < value->field1.count; i++ ) { \
        _SERIALIZE(tai_serialize_ ## field2 (ptr, n, value->field1.list[i]), count, ptr, n);\
        if ( i + 1 < value->field1.count ) { \
            _SERIALIZE(snprintf(ptr, n, ","), count, ptr, n);\
        } \
    } \
    if (option->json ) { \
        _SERIALIZE(snprintf(ptr, n, "]"), count, ptr, n);\
    } \
    return ptr - buffer

int tai_serialize_attribute_value(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ const tai_attr_metadata_t *meta,
        _In_ const tai_attribute_value_t *value,
        _In_ const tai_serialize_option_t *option)
{
    int i, count = 0;
    char *ptr = buffer;

    tai_attr_metadata_t m = *meta;
    m.attrvaluetype = meta->attrlistvaluetype;
    m.attrlistvaluetype = TAI_ATTR_VALUE_TYPE_UNSPECIFIED;

    switch ( meta->attrvaluetype ) {
    case TAI_ATTR_VALUE_TYPE_BOOLDATA:
        return tai_serialize_bool(ptr, n, value->booldata);
    case TAI_ATTR_VALUE_TYPE_CHARDATA:
        return tai_serialize_chardata(ptr, n, value->chardata);
    case TAI_ATTR_VALUE_TYPE_U8:
        return tai_serialize_uint8(ptr, n, value->u8);
    case TAI_ATTR_VALUE_TYPE_S8:
        return tai_serialize_int8(ptr, n, value->s8);
    case TAI_ATTR_VALUE_TYPE_U16:
        return tai_serialize_uint16(ptr, n, value->u16);
    case TAI_ATTR_VALUE_TYPE_S16:
        return tai_serialize_int16(ptr, n, value->s16);
    case TAI_ATTR_VALUE_TYPE_U32:
        return tai_serialize_uint32(ptr, n, value->u32);
    case TAI_ATTR_VALUE_TYPE_S32:
        if ( meta->isenum ) {
            return tai_serialize_enum(ptr, n, meta->enummetadata, value->s32, option);
        }
        return tai_serialize_int32(ptr, n, value->s32);
    case TAI_ATTR_VALUE_TYPE_U64:
        return tai_serialize_uint64(ptr, n, value->u64);
    case TAI_ATTR_VALUE_TYPE_S64:
        return tai_serialize_int64(ptr, n, value->s64);
    case TAI_ATTR_VALUE_TYPE_FLT:
        return tai_serialize_float(ptr, n, value->flt);
    case TAI_ATTR_VALUE_TYPE_PTR:
        TAI_META_LOG_WARN("pointer serialization is not implemented");
        return TAI_SERIALIZE_ERROR;
    case TAI_ATTR_VALUE_TYPE_OID:
        return tai_serialize_object_id(ptr, n, value->oid);
    case TAI_ATTR_VALUE_TYPE_OBJLIST:
        _TAI_SERIALIZE_LIST(objlist, object_id);
    case TAI_ATTR_VALUE_TYPE_CHARLIST:
        if ( value->charlist.count > n ) {
            return value->charlist.count;
        }
        memcpy(ptr, value->charlist.list, value->charlist.count);
        return value->charlist.count;
    case TAI_ATTR_VALUE_TYPE_U8LIST:
        _TAI_SERIALIZE_LIST(u8list, uint8);
    case TAI_ATTR_VALUE_TYPE_S8LIST:
        _TAI_SERIALIZE_LIST(s8list, int8);
    case TAI_ATTR_VALUE_TYPE_U16LIST:
        _TAI_SERIALIZE_LIST(u16list, uint16);
    case TAI_ATTR_VALUE_TYPE_S16LIST:
        _TAI_SERIALIZE_LIST(s16list, int16);
    case TAI_ATTR_VALUE_TYPE_U32LIST:
        _TAI_SERIALIZE_LIST(u32list, int32);
    case TAI_ATTR_VALUE_TYPE_S32LIST:
        if ( option->json ) {
            _SERIALIZE(snprintf(ptr, n, "["), count, ptr, n);
        }
        if ( meta->isenum ) {
            for ( i = 0; i < value->s32list.count; i++ ) {
                _SERIALIZE(tai_serialize_enum(ptr, n, meta->enummetadata, value->s32list.list[i], option), count, ptr, n);
                if ( i + 1 < value->s32list.count ) {
                    if ( option->json ) {
                        count = snprintf(ptr, n, ",");
                    } else {
                        count = snprintf(ptr, n, "|");
                    }
                    if ( count < 0 || count > ( n - (ptr - buffer)) ) {
                        return (ptr - buffer) + count;
                    }
                    ptr += count;
                    n -= count;
                }
            }
        } else {
            for ( i = 0; i < value->s32list.count; i++ ) {
                _SERIALIZE(tai_serialize_int32(ptr, n, value->s32list.list[i]), count, ptr, n);
                if ( i + 1 < value->s32list.count ) {
                    _SERIALIZE(snprintf(ptr, n, ","), count, ptr, n);
                }
            }
        }
        if ( option->json ) {
            _SERIALIZE(snprintf(ptr, n, "]"), count, ptr, n);
        }
        return ptr - buffer;
    case TAI_ATTR_VALUE_TYPE_FLOATLIST:
        _TAI_SERIALIZE_LIST(floatlist, float);
    case TAI_ATTR_VALUE_TYPE_U32RANGE:
        _SERIALIZE(snprintf(ptr, n, "%u,%u", value->u32range.min, value->u32range.max), count, ptr, n);
        return ptr - buffer;
    case TAI_ATTR_VALUE_TYPE_S32RANGE:
        _SERIALIZE(snprintf(ptr, n, "%d,%d", value->s32range.min, value->s32range.max), count, ptr, n);
        return ptr - buffer;
    case TAI_ATTR_VALUE_TYPE_OBJMAPLIST:
        TAI_META_LOG_WARN("objmaplist serialization is not implemented");
        return TAI_SERIALIZE_ERROR;
    case TAI_ATTR_VALUE_TYPE_ATTRLIST:
        if ( option->json ) {
            _SERIALIZE(snprintf(ptr, n, "["), count, ptr, n);
        }
        for ( i = 0; i < value->attrlist.count; i++ ) {
            _SERIALIZE(tai_serialize_attribute_value(ptr, n, &m, &value->attrlist.list[i], option), count, ptr, n);
            if ( i != (value->attrlist.count - 1)) {
                _SERIALIZE(snprintf(ptr, n, ", "), count, ptr, n);
            }
        }
        if ( option->json ) {
            _SERIALIZE(snprintf(ptr, n, "]"), count, ptr, n);
        }
        return ptr - buffer;
    default:
        TAI_META_LOG_WARN("unknown attr value type");
    }
    return TAI_SERIALIZE_ERROR;
}

int _tai_serialize_attribute(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ const tai_attr_metadata_t *meta,
        _In_ const tai_attribute_t *attr,
        _In_ const tai_serialize_option_t *option)
{
    char *ptr = buffer;
    int count = 0;
    const char *name;
    tai_serialize_option_t default_option = {0};
    if ( option == NULL ) {
        option = &default_option;
    }
    if ( option->valueonly ) {
        return tai_serialize_attribute_value(ptr, n, meta, &attr->value, option);
    }

    name = meta->attridname;
    if ( option->human ) {
        name = meta->attridshortname;
    }

    if (option->json) {
        count = snprintf(ptr, n, "{ \"id\": \"%s\", \"value\": ", name);
    } else {
        count = snprintf(ptr, n, "%s | ", name);
    }
    if ( count < 0 || count > n ) {
        return count;
    }
    ptr += count;
    n -= count;

    _SERIALIZE(tai_serialize_attribute_value(ptr, n, meta, &attr->value, option), count, ptr, n);

    if (option->json) {
        _SERIALIZE(snprintf(ptr, n, "}"), count, ptr, n);
    }
    return ptr - buffer;
}

int tai_serialize_attribute(
        _Out_ char *buffer,
        _In_ size_t n,
        _In_ const tai_attr_metadata_t *meta,
        _In_ const tai_attribute_t *attr,
        _In_ const tai_serialize_option_t *option)
{
    int ret =_tai_serialize_attribute(buffer, n, meta, attr, option);
    if ( ret < n ) {
        return ret;
    }
    char *tmp = NULL, *tmp2;
    for ( int i = 0; i < 16; i++ ) {
        n = n*2;
        tmp2 = realloc(tmp, n);
        if ( tmp2 == NULL ) {
            TAI_META_LOG_WARN("realloc failed");
            return -1;
        }
        tmp = tmp2;
        ret = _tai_serialize_attribute(tmp, n, meta, attr, option);
        if ( ret < n ) {
            return ret;
        }
    }
    free(tmp);
    TAI_META_LOG_WARN("attribute is too big to serialize: bigger than %ld bytes", n*16);
    return -1;
}

int tai_deserialize_attribute(
        _In_ const char *buffer,
        _In_ const tai_attr_metadata_t *meta,
        _Out_ tai_attribute_t *attribute,
        _In_ const tai_serialize_option_t *option)
{
    TAI_META_LOG_WARN("not implemented");
    return TAI_SERIALIZE_ERROR;
}

int tai_deserialize_attribute_value(
        _In_ const char *buffer,
        _In_ const tai_attr_metadata_t *meta,
        _Out_ tai_attribute_value_t *value,
        _In_ const tai_serialize_option_t *option)
{
    switch ( meta->attrvaluetype ) {
    case TAI_ATTR_VALUE_TYPE_BOOLDATA:
        return tai_deserialize_bool(buffer, &value->booldata);
    case TAI_ATTR_VALUE_TYPE_CHARDATA:
        return tai_deserialize_chardata(buffer, value->chardata);
    case TAI_ATTR_VALUE_TYPE_U8:
        return tai_deserialize_uint8(buffer, &value->u8);
    case TAI_ATTR_VALUE_TYPE_S8:
        return tai_deserialize_int8(buffer, &value->s8);
    case TAI_ATTR_VALUE_TYPE_U16:
        return tai_deserialize_uint16(buffer, &value->u16);
    case TAI_ATTR_VALUE_TYPE_S16:
        return tai_deserialize_int16(buffer, &value->s16);
    case TAI_ATTR_VALUE_TYPE_U32:
        return tai_deserialize_uint32(buffer, &value->u32);
    case TAI_ATTR_VALUE_TYPE_S32:
        if ( meta->isenum ) {
            return tai_deserialize_enum(buffer, meta->enummetadata, &value->s32, option);
        }
        return tai_deserialize_int32(buffer, &value->s32);
    case TAI_ATTR_VALUE_TYPE_U64:
        return tai_deserialize_uint64(buffer, &value->u64);
    case TAI_ATTR_VALUE_TYPE_S64:
        return tai_deserialize_int64(buffer, &value->s64);
    case TAI_ATTR_VALUE_TYPE_FLT:
        return tai_deserialize_float(buffer, &value->flt);
    case TAI_ATTR_VALUE_TYPE_PTR:
        TAI_META_LOG_WARN("pointer serialization is not implemented");
        return TAI_SERIALIZE_ERROR;
    }
    return TAI_SERIALIZE_ERROR;
}