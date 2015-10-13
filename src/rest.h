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


/**
 * Supported HTTP commands
 */
typedef enum {
        REST_GET,
        REST_POST,
} rest_cmd_t;


/**
 * List of URLs with built-in round-robin.
 */
typedef struct url_list_s {
        char **urls;          /* URLs */
        int    cnt;           /* Number of URLs in 'urls' */
        int    idx;           /* Next URL to try */
        char  *str;           /* Original string (copy), urls[..] points here */
        int    max_len;       /* Longest URL's length */
} url_list_t;


/**
 * Parse a comma-separated list of URLs and store them in the provided 'ul'.
 * Returns the number of parsed URLs.
 */
int url_list_parse (url_list_t *ul, const char *urls);


/**
 * Free resources associated with an URL list.
 */
void url_list_clear (url_list_t *ul);


/**
 * REST response object, contains the response code, payload, errors, etc.
 */
typedef struct rest_response_s {
        int   size;         /* payload buffer size */
        int   len;          /* actual payload length so far */
        char *payload;      /* Response payload (allocated) */
        long  code;         /* HTTP Response code */
        char *errstr;       /* Error string (allocated) */
} rest_response_t;


/**
 * Check if response indicates failure.
 *
 * For server-reported failures the error string will be in ->payload
 * (not null terminated),
 * while for locally reported failures (e.g., connection failures, etc)
 * the error string will be in ->errstr.
 *
 * Local failures will have error ->code -1 while server-reported failures
 * will have ->code > 0
 */
#define rest_response_failed(rr)  ((rr)->code < 100 || (rr)->code > 299)


/**
 * Writes the response error string to `errstr`.
 * Should only be used on rest_response_t where rest_response_failed()
 * returned true.
 */
char *rest_response_strerror (const rest_response_t *rr,
                              char *errstr, int errstr_size);

/**
 * Destroy a response object and free all associated resources.
 */
void rest_response_destroy (rest_response_t *rr);

/**
 * REST GET request.
 *
 * `ul` is a list of URLs to which `url_path_fmt + ...` will be appended.
 * The URLs will be tried in a round-robin fashion until one returns
 * a succesful response or all URLs have been exhausted.
 *
 * Returns a response object, use rest_response_failed() to check if
 * the response contains an error.
 *
 * This is a blocking call.
 */
rest_response_t *rest_get (url_list_t *ul, const char *url_path_fmt, ...);


/* REST PUT request.
 *
 * Same semantics as `rest_get()` but POSTs `payload` of `size` bytes.
 */
rest_response_t *rest_post (url_list_t *ul,
                            const void *payload, int size,
                            const char *url_path_fmt, ...);


