#ifndef NGX_HTTP_SRCACHE_UTIL_H
#define NGX_HTTP_SRCACHE_UTIL_H

#include "ngx_http_srcache_filter_module.h"


#define ngx_http_srcache_method_name(m) { sizeof(m) - 1, (u_char *) m " " }

#define ngx_http_srcache_strcmp_const(a, b) \
        ngx_strncmp(a, b, sizeof(b) - 1)

ngx_str_t  ngx_http_srcache_content_length_header_key;

ngx_str_t  ngx_http_srcache_get_method;
ngx_str_t  ngx_http_srcache_put_method;
ngx_str_t  ngx_http_srcache_post_method;
ngx_str_t  ngx_http_srcache_head_method;
ngx_str_t  ngx_http_srcache_copy_method;
ngx_str_t  ngx_http_srcache_move_method;
ngx_str_t  ngx_http_srcache_lock_method;
ngx_str_t  ngx_http_srcache_mkcol_method;
ngx_str_t  ngx_http_srcache_trace_method;
ngx_str_t  ngx_http_srcache_delete_method;
ngx_str_t  ngx_http_srcache_unlock_method;
ngx_str_t  ngx_http_srcache_options_method;
ngx_str_t  ngx_http_srcache_propfind_method;
ngx_str_t  ngx_http_srcache_proppatch_method;

ngx_int_t ngx_http_srcache_parse_method_name(ngx_str_t **method_name_ptr);
void ngx_http_srcache_discard_bufs(ngx_pool_t *pool, ngx_chain_t *in);
ngx_int_t ngx_http_srcache_adjust_subrequest(ngx_http_request_t *sr,
        ngx_http_srcache_parsed_request_t *parsed_sr);
ngx_int_t ngx_http_srcache_add_copy_chain(ngx_pool_t *pool,
        ngx_chain_t **chain, ngx_chain_t *in);
ngx_int_t ngx_http_srcache_post_request_at_head(ngx_http_request_t *r,
        ngx_http_posted_request_t *pr);


#endif /* NGX_HTTP_SRCACHE_UTIL_H */

