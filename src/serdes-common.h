/**
 * Copyright 2015 Confluent Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once


/*******************************************************************************
 *
 * Errors
 *
 ******************************************************************************/

/**
 * Error codes
 */
typedef enum {
        SERDES_ERR_OK = 0,            /* No error */
        SERDES_ERR_CONF_UNKNOWN,      /* Unknown configuration property */
        SERDES_ERR_CONF_INVALID,      /* Invalid configuration property value */
        SERDES_ERR_FRAMING_INVALID,   /* Invalid payload framing */
        SERDES_ERR_SCHEMA_LOAD,       /* Schema load failed */
        SERDES_ERR_PAYLOAD_INVALID,   /* Payload is invalid */
        SERDES_ERR_SCHEMA_MISMATCH,   /* Object does not match schema */
        SERDES_ERR_SCHEMA_REQUIRED,   /* Schema required to perform operation */
        SERDES_ERR_SERIALIZER,        /* Serializer failed */
        SERDES_ERR_BUFFER_SIZE        /* Inadequate buffer size */
} serdes_err_t;


#ifdef _MSC_VER
/* MSVC Win32 DLL symbol exports */
#undef SERDES_EXPORT
#ifdef SERDES_DLL_EXPORTS /* Set when building DLL */
#define SERDES_EXPORT __declspec(dllexport)
#else
#define SERDES_EXPORT __declspec(dllimport)
#endif

#else
#define SERDES_EXPORT
#endif

