#ifndef NGX_HTTP_SRCACHE_FILTER_MODULE_H
#define NGX_HTTP_SRCACHE_FILTER_MODULE_H


#include <ngx_core.h>


typedef struct {
    ngx_int_t                   method;
    ngx_http_complex_value_t    location;
    ngx_http_complex_value_t    args;
} ngx_http_srcache_request_t;

typedef struct {
    ngx_http_srcache_request_t      *fetch;
    ngx_http_srcache_request_t      *store;

    size_t        buf_size;
} ngx_http_srcache_conf_t;

typedef struct {
    ngx_flag_t      waiting_subrequest:1;
    ngx_flag_t      request_done:1;
    ngx_flag_t      from_cache:1;
    ngx_flag_t      in_fetch_subrequest:1;
    ngx_flag_t      ignore_body:1;
    ngx_flag_t      parsing_cached_headers:1;
} ngx_http_srcache_ctx_t;


#endif

