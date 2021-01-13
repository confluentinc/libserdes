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
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <curl/curl.h>

#include "rest.h"
#include "tinycthread.h"

static once_flag rest_global_init_once = ONCE_FLAG_INIT;

static void unittest_url_encode (void);

/**
 * Once-per-runtime init of REST framework
 */
static void rest_init_cb (void) {
        CURLcode ccode;
        ccode = curl_global_init(CURL_GLOBAL_ALL);
        if (ccode != CURLE_OK)
                fprintf(stderr, "libserdes: curl_global_init failed: %s\n",
                        curl_easy_strerror(ccode));

        if (0) /* Enable for unittests */
                unittest_url_encode();
}

static void rest_init (void) {
        call_once(&rest_global_init_once, rest_init_cb);
}


/**
 * @brief URL-encode auth portition (if any) of the URL.
 *
 * @returns a newly allocated URL.
 */
char *url_encode (const char *orig) {
        const char *s = orig;
        const char *t1, *t2;
        const char *proto = "";
        char *url;

        /* If it seems like the URL has auth fields (user:password@),
         * URL-encode them separately and rebuild the URL.
         * This is not completely fool proof since the user:password fields
         * can contain any character. */

        /* Get past the URL proto, if any. */
        if (!strncmp(s, "http://", strlen("http://")))
                proto = "http://";
        else if (!strncmp(s, "https://", strlen("https://")))
                proto = "https://";

        s += strlen(proto);

        /* Look for "username:password@.."
         *          s^       ^t1      ^t2 */
        if ((t2 = strchr(s, '@')) &&
            (t1 = strchr(s, ':')) &&
            t1 < t2) {
                CURL *curl = curl_easy_init();
                int user_len = (int)(t1-s);
                int pass_len = (int)(t2 - (t1+1));
                char *user = user_len > 0 ?
                        curl_easy_escape(curl, s, user_len) : "";
                char *pass = pass_len > 0 ?
                        curl_easy_escape(curl, t1+1, pass_len) : "";
                size_t size;

                s = t2+1;
                size = strlen(proto) + strlen(user) + 1 + strlen(pass) + 1 +
                        strlen(s) + 1;

                /* Re-construct URL with now escaped auth */
                url = malloc(size);
                snprintf(url, size, "%s%s:%s@%s", proto, user, pass, s);
                if (user_len > 0)
                        curl_free(user);
                if (pass_len > 0)
                        curl_free(pass);
                curl_easy_cleanup(curl);
                return url;
        }

        return strdup(orig);
}


/**
 * @brief Unittests for url_encode().
 *
 * Enable in rest_init_cb() and run any of the example programs
 * to excercise.
 */
static void unittest_url_encode (void) {
        /* Tuples of input urls and expected output */
        const char *test[] = {
                "", "",
                "http://", "http://",
                "localhost:1234/", "localhost:1234/",

                "http://aba:laba@domain.com/with/a1",
                "http://aba:laba@domain.com/with/a1",

                "https://\\cpt:h$dd0ck!@mysite__.com",
                "https://%5Ccpt:h%24dd0ck%21@mysite__.com",

                ":!@.", ":%21@.",

                "https://:@empty.com", "https://:@empty.com",
                "https://a:@empty.com", "https://a:@empty.com",
                "https://:b@empty.com", "https://:b@empty.com",
                "https://:@", "https://:@",

                NULL, NULL,
        };
        int i;
        int fails = 0;

        for (i = 0 ; test[i] ; i += 2) {
                const char *input = test[i];
                const char *exp = test[i+1];
                char *encoded;

                encoded = url_encode(input);
                if (strcmp(encoded, exp)) {
                        fprintf(stderr, "%s: expected %s for %s, not %s\n",
                                __FUNCTION__, exp, input, encoded);
                        fails++;
                }

                free(encoded);
        }

        assert(!fails);

        fprintf(stderr, "%s PASSED\n", __FUNCTION__);
}


int url_list_parse (url_list_t *ul, const char *urls) {
        char *s, *s_orig;
        char *t;

        ul->str     = strdup(urls);
        ul->cnt     = 0;
        ul->idx     = 0;
        ul->max_len = 0;
        ul->urls    = NULL;

        s_orig = strdup(ul->str);
        s = s_orig;

        while (*s) {
                int len;

                while (*s == ' ')
                        s++;

                if ((t = strchr(s, ','))) {
                        *t = '\0';
                        t++;
                }
                else
                        t = s + strlen(s);

                ul->urls = realloc(ul->urls, sizeof(*ul->urls) * (++(ul->cnt)));

                /* URL-encode auth fields, if they exist. */
                ul->urls[ul->cnt-1] = url_encode(s);

                if ((len = strlen(ul->urls[ul->cnt-1])) > ul->max_len)
                        ul->max_len = len;

                s = t;
        }

        free(s_orig);

        return ul->cnt;
}

void url_list_clear (url_list_t *ul) {
        int i;

        for (i = 0 ; i < ul->cnt ; i++)
                free(ul->urls[i]);
        if (ul->urls)
                free(ul->urls);
        if (ul->str)
                free(ul->str);
}






/**
 * Set response result.
 */
static void rest_response_set_result (rest_response_t *rr, int resp_code,
                                      const char *fmt, ...) {
        va_list ap;

        rr->code = resp_code;
        if (fmt) {
                int r;
		va_list ap2;

                va_start(ap, fmt);
		va_copy(ap2, ap);
                r = vsnprintf(NULL, 0, fmt, ap2);
		va_end(ap2);

                rr->errstr = malloc(r+1);
                vsnprintf(rr->errstr, r+1, fmt, ap);;
		va_end(ap);
        }
}

/**
 * Write rest_response_t error to string.
 */
char *rest_response_strerror (const rest_response_t *rr,
                             char *errstr, int errstr_size) {
        if (rr->errstr)
                snprintf(errstr, errstr_size,
                         "REST request failed (code %ld): %s",
                         rr->code, rr->errstr);
        else
                snprintf(errstr, errstr_size,
                         "REST request failed (code %ld): %.*s",
                         rr->code, rr->len, rr->payload);

        return errstr;
}


/**
 * Reset buffer pointers for reuse. Will not free any memory.
 */
static void rest_response_reset (rest_response_t *rr) {
        rr->code = 0;
        if (rr->errstr) {
                free(rr->errstr);
                rr->errstr = NULL;
        }
        rr->len = 0;
}

/**
 * Grow response buffer by (at least) 'add_size'
 */
static void rest_response_grow (rest_response_t *rr, int add_size) {
        /* Grow by at least the double size */
        if (add_size < rr->size)
                add_size = rr->size;
        rr->size += add_size;
        rr->payload = realloc(rr->payload, rr->size);
}

/**
 * Destroy and free a response
 */
void rest_response_destroy (rest_response_t *rr) {
        if (rr->payload)
                free(rr->payload);
        if (rr->errstr)
                free(rr->errstr);
        free(rr);
}

/**
 * Create new response handle.
 */
static rest_response_t *rest_response_new (int initial_size) {
        rest_response_t *rr;

        rr = calloc(1, sizeof(*rr));
        if (initial_size)
                rest_response_grow(rr, initial_size);

        return rr;
}


/**
 * cURL write callback for writing server-sent data to the response buffer
 */
static size_t rest_curl_write_cb (char *ptr, size_t size, size_t nmemb,
                                  void *userdata) {
        rest_response_t *rr = userdata;

        size *= nmemb;

        if (rr->len + (int)size > rr->size)
                rest_response_grow(rr, size);

        memcpy(rr->payload+rr->len, ptr, size);
        rr->len += size;

        return size;
}





/**
 * (low level) REST requester.
 * The response will be updated with an error or the response payload.
 */
static CURLcode rest_req_curl (CURL *curl, rest_response_t *rr) {
        CURLcode ccode;

        ccode = curl_easy_perform(curl);
        if (ccode != CURLE_OK) {
                rest_response_set_result(rr, -1,
                                         "HTTP request failed: %s",
                                         curl_easy_strerror(ccode));
        } else {
                if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
                                      &rr->code) != CURLE_OK)
                        rest_response_set_result(rr, CURLE_HTTP_RETURNED_ERROR,
                                                 "No HTTP response code");
                else
                        rest_response_set_result(rr, rr->code, NULL);
        }

        return ccode;
}


/**
 * Perform 'cmd' (GET,POST,PUT,..) request to URLs on list 'ul'
 * by appending 'url_path_fmt' to each URL.
 * The URLs in 'ul' will be tried in a round-robin fashion until one
 * returns a succesful reply.
 * For POST & PUT, 'payload' and 'size' is the transmitted payload.
 *
 * Returns a response handle which needs to be checked for error.
 */
static rest_response_t *rest_req (url_list_t *ul, rest_cmd_t cmd,
                                  const void *payload, int size,
                                  const char *url_path_fmt, va_list ap) {

        CURL *curl;
        CURLcode ccode;
        rest_response_t *rr;
        struct curl_slist *hdrs = NULL;
        char *tmpurl;
        int start_idx;
        char *url_path;
        int url_path_len;
        va_list ap2;
        const int debug = 0;

        /* Initialize rest, once */
        rest_init();

        /* Construct URL suffix */
        va_copy(ap2, ap);
        url_path_len = vsnprintf(NULL, 0, url_path_fmt, ap);
        url_path = alloca(url_path_len+1);
        vsnprintf(url_path, url_path_len+1, url_path_fmt, ap2);

        /* Create cURL handle */
        curl = curl_easy_init();

        /* Response holder */
        rr = rest_response_new(0);

#define do_curl_setopt(curl,opt,val...) do {                            \
                CURLcode _ccode = curl_easy_setopt(curl, opt, val);     \
                if (_ccode != CURLE_OK) {                               \
                        rest_response_set_result(rr, -1,                \
                                                 "curl: setopt %s failed: %s", \
                                                 #opt,                  \
                                                 curl_easy_strerror(_ccode)); \
                        if (hdrs)                                       \
                                curl_slist_free_all(hdrs);              \
                        curl_easy_cleanup(curl);                        \
                        return rr;                                      \
                }                                                       \
         } while (0)


        /* Set up cURL request */
        hdrs = curl_slist_append(hdrs, "Accept: application/vnd.schemaregistry.v1+json");
        hdrs = curl_slist_append(hdrs, "Content-Type: application/vnd.schemaregistry.v1+json");
        hdrs = curl_slist_append(hdrs, "Charsets: utf-8");
        do_curl_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        if (debug)
                do_curl_setopt(curl, CURLOPT_VERBOSE, (long)1);
        do_curl_setopt(curl, CURLOPT_USERAGENT, "libserdes");
        do_curl_setopt(curl, CURLOPT_WRITEFUNCTION, rest_curl_write_cb);
        do_curl_setopt(curl, CURLOPT_WRITEDATA, rr);

        switch (cmd)
        {
        case REST_GET:
                do_curl_setopt(curl, CURLOPT_HTTPGET, 1);
                break;

        case REST_POST:
                do_curl_setopt(curl, CURLOPT_POST, 1);
                do_curl_setopt(curl, CURLOPT_POSTFIELDS, payload);
                do_curl_setopt(curl, CURLOPT_POSTFIELDSIZE, size);
                break;
        }


        /* Try each URL in the URL list until one works. */
        ccode = CURLE_URL_MALFORMAT;
        tmpurl = alloca(ul->max_len + 1 + strlen(url_path) + 1);
        start_idx = ul->idx;
        do {
                /*  Handle the '/' in url
                 *  When schema registry url is http://127.0.0.1:8081/,
                 *  it returns 404 error code. We need to remove the
                 *  the last redundant '/' in the url.
                 */
                size_t url_len = strlen(ul->urls[ul->idx]);
                while (url_len > 0 && ul->urls[ul->idx][url_len - 1] == '/')
                    url_len --;
                sprintf(tmpurl, "%.*s%s", (int)url_len, ul->urls[ul->idx], url_path);
                do_curl_setopt(curl, CURLOPT_URL, tmpurl);

                rest_response_reset(rr);

                /* Perform request */
                ccode = rest_req_curl(curl, rr);
                if (ccode == CURLE_OK)
                        break;

                /* Try next */
                ul->idx = (ul->idx + 1) % ul->cnt;
        } while (ul->idx != start_idx);

        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);
        return rr;
}



rest_response_t *rest_get (url_list_t *ul, const char *url_path_fmt, ...) {
        rest_response_t *rr;
        va_list ap;

        va_start(ap, url_path_fmt);
        rr = rest_req(ul, REST_GET, NULL, 0, url_path_fmt, ap);
        va_end(ap);

        return rr;
}


rest_response_t *rest_post (url_list_t *ul,
                            const void *payload, int size,
                            const char *url_path_fmt, ...) {
        rest_response_t *rr;
        va_list ap;

        va_start(ap, url_path_fmt);
        rr = rest_req(ul, REST_POST, payload, size, url_path_fmt, ap);
        va_end(ap);

        return rr;
}



