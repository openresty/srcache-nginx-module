#define DDEBUG 0
#include "ddebug.h"

#include "ngx_http_srcache_util.h"


static ngx_int_t ngx_http_srcache_set_content_length_header(
        ngx_http_request_t *r, off_t len);


ngx_str_t  ngx_http_srcache_content_length_header_key =
        ngx_string("Content-Length");
ngx_str_t  ngx_http_srcache_get_method =
        ngx_http_srcache_method_name("GET");
ngx_str_t  ngx_http_srcache_put_method =
        ngx_http_srcache_method_name("PUT");
ngx_str_t  ngx_http_srcache_post_method =
        ngx_http_srcache_method_name("POST");
ngx_str_t  ngx_http_srcache_head_method =
        ngx_http_srcache_method_name("HEAD");
ngx_str_t  ngx_http_srcache_copy_method =
        ngx_http_srcache_method_name("COPY");
ngx_str_t  ngx_http_srcache_move_method =
        ngx_http_srcache_method_name("MOVE");
ngx_str_t  ngx_http_srcache_lock_method =
        ngx_http_srcache_method_name("LOCK");
ngx_str_t  ngx_http_srcache_mkcol_method =
        ngx_http_srcache_method_name("MKCOL");
ngx_str_t  ngx_http_srcache_trace_method =
        ngx_http_srcache_method_name("TRACE");
ngx_str_t  ngx_http_srcache_delete_method =
        ngx_http_srcache_method_name("DELETE");
ngx_str_t  ngx_http_srcache_unlock_method =
        ngx_http_srcache_method_name("UNLOCK");
ngx_str_t  ngx_http_srcache_options_method =
        ngx_http_srcache_method_name("OPTIONS");
ngx_str_t  ngx_http_srcache_propfind_method =
        ngx_http_srcache_method_name("PROPFIND");
ngx_str_t  ngx_http_srcache_proppatch_method =
        ngx_http_srcache_method_name("PROPPATCH");


void
ngx_http_srcache_discard_bufs(ngx_pool_t *pool, ngx_chain_t *in)
{
    ngx_chain_t         *cl;

    for (cl = in; cl; cl = cl->next) {
        cl->buf->pos = cl->buf->last;
    }
}


ngx_int_t
ngx_http_srcache_parse_method_name(ngx_str_t **method_name_ptr)
{
    const ngx_str_t* method_name = *method_name_ptr;

    switch (method_name->len) {
    case 3:
        if (ngx_http_srcache_strcmp_const(method_name->data, "GET") == 0) {
            *method_name_ptr = &ngx_http_srcache_get_method;
            return NGX_HTTP_GET;
            break;
        }

        if (ngx_http_srcache_strcmp_const(method_name->data, "PUT") == 0) {
            *method_name_ptr = &ngx_http_srcache_put_method;
            return NGX_HTTP_PUT;
            break;
        }

        return NGX_HTTP_UNKNOWN;
        break;

    case 4:
        if (ngx_http_srcache_strcmp_const(method_name->data, "POST") == 0) {
            *method_name_ptr = &ngx_http_srcache_post_method;
            return NGX_HTTP_POST;
            break;
        }
        if (ngx_http_srcache_strcmp_const(method_name->data, "HEAD") == 0) {
            *method_name_ptr = &ngx_http_srcache_head_method;
            return NGX_HTTP_HEAD;
            break;
        }
        if (ngx_http_srcache_strcmp_const(method_name->data, "COPY") == 0) {
            *method_name_ptr = &ngx_http_srcache_copy_method;
            return NGX_HTTP_COPY;
            break;
        }
        if (ngx_http_srcache_strcmp_const(method_name->data, "MOVE") == 0) {
            *method_name_ptr = &ngx_http_srcache_move_method;
            return NGX_HTTP_MOVE;
            break;
        }
        if (ngx_http_srcache_strcmp_const(method_name->data, "LOCK") == 0) {
            *method_name_ptr = &ngx_http_srcache_lock_method;
            return NGX_HTTP_LOCK;
            break;
        }
        return NGX_HTTP_UNKNOWN;
        break;

    case 5:
        if (ngx_http_srcache_strcmp_const(method_name->data, "MKCOL") == 0) {
            *method_name_ptr = &ngx_http_srcache_mkcol_method;
            return NGX_HTTP_MKCOL;
            break;
        }
        if (ngx_http_srcache_strcmp_const(method_name->data, "TRACE") == 0) {
            *method_name_ptr = &ngx_http_srcache_trace_method;
            return NGX_HTTP_TRACE;
            break;
        }
        return NGX_HTTP_UNKNOWN;
        break;

    case 6:
        if (ngx_http_srcache_strcmp_const(method_name->data, "DELETE") == 0) {
            *method_name_ptr = &ngx_http_srcache_delete_method;
            return NGX_HTTP_DELETE;
            break;
        }

        if (ngx_http_srcache_strcmp_const(method_name->data, "UNLOCK") == 0) {
            *method_name_ptr = &ngx_http_srcache_unlock_method;
            return NGX_HTTP_UNLOCK;
            break;
        }
        return NGX_HTTP_UNKNOWN;
        break;

    case 7:
        if (ngx_http_srcache_strcmp_const(method_name->data, "OPTIONS") == 0) {
            *method_name_ptr = &ngx_http_srcache_options_method;
            return NGX_HTTP_OPTIONS;
            break;
        }
        return NGX_HTTP_UNKNOWN;
        break;

    case 8:
        if (ngx_http_srcache_strcmp_const(method_name->data, "PROPFIND") == 0) {
            *method_name_ptr = &ngx_http_srcache_propfind_method;
            return NGX_HTTP_PROPFIND;
            break;
        }
        return NGX_HTTP_UNKNOWN;
        break;

    case 9:
        if (ngx_http_srcache_strcmp_const(method_name->data, "PROPPATCH")
                == 0)
        {
            *method_name_ptr = &ngx_http_srcache_proppatch_method;
            return NGX_HTTP_PROPPATCH;
            break;
        }
        return NGX_HTTP_UNKNOWN;
        break;

    default:
        return NGX_HTTP_UNKNOWN;
        break;
    }

    return NGX_HTTP_UNKNOWN;
}


ngx_int_t
ngx_http_srcache_adjust_subrequest(ngx_http_request_t *sr,
        ngx_http_srcache_parsed_request_t *parsed_sr)
{
    ngx_http_core_main_conf_t  *cmcf;
    ngx_http_request_t         *r;
    ngx_http_request_body_t    *body;
    ngx_int_t                   rc;

    sr->method      = parsed_sr->method;
    sr->method_name = parsed_sr->method_name;

    r = sr->parent;

    dd("subrequest method: %d %.*s", (int) sr->method,
            (int) sr->method_name.len, sr->method_name.data);

    sr->header_in = r->header_in;

#if 1
    /* XXX work-around a bug in ngx_http_subrequest */
    if (r->headers_in.headers.last == &r->headers_in.headers.part) {
        sr->headers_in.headers.last = &sr->headers_in.headers.part;
    }
#endif

    /* we do not inherit the parent request's variables */
    cmcf = ngx_http_get_module_main_conf(sr, ngx_http_core_module);
    sr->variables = ngx_pcalloc(sr->pool, cmcf->variables.nelts
                                        * sizeof(ngx_http_variable_value_t));

    if (sr->variables == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    body = parsed_sr->request_body;
    if (body) {
        sr->request_body = body;

        rc = ngx_http_srcache_set_content_length_header(sr,
                body->buf ? ngx_buf_size(body->buf) : 0);

        if (rc != NGX_OK) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }
    }

    return NGX_OK;
}


ngx_int_t
ngx_http_srcache_add_copy_chain(ngx_pool_t *pool, ngx_chain_t **chain,
        ngx_chain_t *in)
{
    ngx_chain_t     *cl, **ll;
    size_t           len;

    ll = chain;

    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NGX_ERROR;
        }

        if (ngx_buf_special(in->buf)) {
            cl->buf = in->buf;

        } else {
            if (ngx_buf_in_memory(in->buf)) {
                len = ngx_buf_size(in->buf);
                cl->buf = ngx_create_temp_buf(pool, len);
                if (cl->buf == NULL) {
                    return NGX_ERROR;
                }

                dd("buf: %.*s", (int) len, in->buf->pos);

                cl->buf->last = ngx_copy(cl->buf->pos, in->buf->pos, len);

            } else {
                return NGX_ERROR;
            }
        }

        *ll = cl;
        ll = &cl->next;
        in = in->next;
    }

    *ll = NULL;

    return NGX_OK;
}


ngx_int_t
ngx_http_srcache_post_request_at_head(ngx_http_request_t *r,
        ngx_http_posted_request_t *pr)
{
    dd_enter();

    if (pr == NULL) {
        pr = ngx_palloc(r->pool, sizeof(ngx_http_posted_request_t));
        if (pr == NULL) {
            return NGX_ERROR;
        }
    }

    pr->request = r;
    pr->next = r->main->posted_requests;
    r->main->posted_requests = pr;

    return NGX_OK;
}


static ngx_int_t
ngx_http_srcache_set_content_length_header(ngx_http_request_t *r, off_t len)
{
    ngx_table_elt_t                 *h, *header;
    u_char                          *p;
    ngx_list_part_t                 *part;
    ngx_http_request_t              *pr;
    ngx_uint_t                       i;

    r->headers_in.content_length_n = len;

    if (ngx_list_init(&r->headers_in.headers, r->pool, 20,
                sizeof(ngx_table_elt_t)) != NGX_OK) {
        return NGX_ERROR;
    }

    h = ngx_list_push(&r->headers_in.headers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    h->key = ngx_http_srcache_content_length_header_key;
    h->lowcase_key = ngx_pnalloc(r->pool, h->key.len);
    if (h->lowcase_key == NULL) {
        return NGX_ERROR;
    }

    ngx_strlow(h->lowcase_key, h->key.data, h->key.len);

    r->headers_in.content_length = h;

    p = ngx_palloc(r->pool, NGX_OFF_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    h->value.data = p;

    h->value.len = ngx_sprintf(h->value.data, "%O", len) - h->value.data;

    h->hash = 1;

    dd("r content length: %.*s",
            (int)r->headers_in.content_length->value.len,
            r->headers_in.content_length->value.data);

    pr = r->parent;

    if (pr == NULL) {
        return NGX_OK;
    }

    /* forward the parent request's all other request headers */

    part = &pr->headers_in.headers.part;
    header = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            header = part->elts;
            i = 0;
        }

        if (header[i].key.len == sizeof("Content-Length") - 1 &&
                ngx_strncasecmp(header[i].key.data, (u_char *) "Content-Length",
                sizeof("Content-Length") - 1) == 0)
        {
            continue;
        }

        h = ngx_list_push(&r->headers_in.headers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        *h = header[i];
    }

    /* XXX maybe we should set those built-in header slot in
     * ngx_http_headers_in_t too? */

    return NGX_OK;
}


ngx_int_t
ngx_http_srcache_request_no_cache(ngx_http_request_t *r)
{
    ngx_table_elt_t                 *h;
    ngx_list_part_t                 *part;
    u_char                          *p;
    u_char                          *last;
    ngx_uint_t                       i;

    part = &r->headers_in.headers.part;
    h = part->elts;

    for (i = 0; /* void */; i++) {

        if (i >= part->nelts) {
            if (part->next == NULL) {
                break;
            }

            part = part->next;
            h = part->elts;
            i = 0;
        }

        if (h[i].key.len == sizeof("Cache-Control") - 1
            && ngx_strncasecmp(h[i].key.data, (u_char *) "Cache-Control",
                sizeof("Cache-Control") - 1) == 0)
        {
            p = h[i].value.data;
            last = p + h[i].value.len;

            if (ngx_strlcasestrn(p, last, (u_char *) "no-cache", 8 - 1) != NULL)
            {
                return NGX_OK;
            }

            continue;
        }

        if (h[i].key.len == sizeof("Pragma") - 1
            && ngx_strncasecmp(h[i].key.data, (u_char *) "Pragma",
                sizeof("Pragma") - 1) == 0)
        {
            p = h[i].value.data;
            last = p + h[i].value.len;

            if (ngx_strlcasestrn(p, last, (u_char *) "no-cache", 8 - 1) != NULL)
            {
                return NGX_OK;
            }
        }
    }

    return NGX_DECLINED;
}


ngx_int_t
ngx_http_srcache_response_no_cache(ngx_http_request_t *r,
        ngx_http_srcache_loc_conf_t *conf)
{
    ngx_table_elt_t   **ccp;
    ngx_uint_t          i;
    u_char             *p, *last;

    ccp = r->headers_out.cache_control.elts;

    if (ccp == NULL) {
        return NGX_DECLINED;
    }

    for (i = 0; i < r->headers_out.cache_control.nelts; i++) {
        if (!ccp[i]->hash) {
            continue;
        }

        p = ccp[i]->value.data;
        last = p + ccp[i]->value.len;

        if (!conf->store_private
            && ngx_strlcasestrn(p, last, (u_char *) "private", 7 - 1) != NULL)
        {
            return NGX_OK;
        }

        if (!conf->store_no_store
            && ngx_strlcasestrn(p, last, (u_char *) "no-store", 8 - 1) != NULL)
        {
            return NGX_OK;
        }

    }

    return NGX_DECLINED;
}

