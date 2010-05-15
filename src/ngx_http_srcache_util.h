#ifndef NGX_HTTP_SRCACHE_UTIL_H
#define NGX_HTTP_SRCACHE_UTIL_H

#include "ngx_http_srcache_filter_module.h"


#define ngx_http_srcache_method_name(m) { sizeof(m) - 1, (u_char *) m " " }


#ifndef ngx_str4cmp

#  if (NGX_HAVE_LITTLE_ENDIAN && NGX_HAVE_NONALIGNED)

#    define ngx_str4cmp(m, c0, c1, c2, c3)                                        \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)

#  else

#    define ngx_str4cmp(m, c0, c1, c2, c3)                                        \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3

#  endif

#endif /* ngx_str4cmp */

#ifndef ngx_str3cmp

#  define ngx_str3cmp(m, c0, c1, c2)                                       \
    m[0] == c0 && m[1] == c1 && m[2] == c2

#endif /* ngx_str3cmp */

#ifndef ngx_str6cmp

#  if (NGX_HAVE_LITTLE_ENDIAN && NGX_HAVE_NONALIGNED)

#    define ngx_str6cmp(m, c0, c1, c2, c3, c4, c5)                                \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && (((uint32_t *) m)[1] & 0xffff) == ((c5 << 8) | c4)

#  else

#    define ngx_str6cmp(m, c0, c1, c2, c3, c4, c5)                                \
    m[0] == c0 && m[1] == c1 && m[2] == c2 && m[3] == c3                      \
        && m[4] == c4 && m[5] == c5

#  endif

#endif /* ngx_str6cmp */

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

