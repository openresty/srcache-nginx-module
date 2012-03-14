#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"

#include "ngx_http_srcache_fetch.h"
#include "ngx_http_srcache_store.h"
#include "ngx_http_srcache_util.h"


static ngx_int_t ngx_http_srcache_fetch_subrequest(ngx_http_request_t *r,
        ngx_http_srcache_loc_conf_t *conf, ngx_http_srcache_ctx_t *ctx);


ngx_int_t
ngx_http_srcache_access_handler(ngx_http_request_t *r)
{
    ngx_str_t                       skip;
    ngx_int_t                       rc;
    ngx_http_srcache_loc_conf_t    *conf;
    ngx_http_srcache_main_conf_t   *smcf;
    ngx_http_srcache_ctx_t         *ctx;
    ngx_chain_t                    *cl;
    size_t                          len;
    unsigned                        no_store;

    /* access phase handlers are skipped in subrequests,
     * so the current request must be a main request */

    conf = ngx_http_get_module_loc_conf(r, ngx_http_srcache_filter_module);

    if (conf->fetch == NULL && conf->store == NULL) {
        dd("bypass: %.*s", (int) r->uri.len, r->uri.data);
        return NGX_DECLINED;
    }

    dd("store defined? %p", conf->store);

    dd("req method: %lu", (unsigned long) r->method);
    dd("cache methods: %lu", (unsigned long) conf->cache_methods);

    if (!(r->method & conf->cache_methods)) {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "srcache_fetch and srcache_store skipped due to request "
                "method %V", &r->method_name);

        return NGX_DECLINED;
    }

    if (conf->req_cache_control
        && ngx_http_srcache_request_no_cache(r, &no_store) == NGX_OK)
    {
        ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "srcache_fetch skipped due to request headers "
                "\"Cache-Control: no-cache\" or \"Pragma: no-cache\"");

        if (!no_store) {
            /* register a ctx to give a chance to srcache_store to run */

            ctx = ngx_pcalloc(r->pool,
                    sizeof(ngx_http_srcache_filter_module));

            if (ctx == NULL) {
                return NGX_HTTP_INTERNAL_SERVER_ERROR;
            }

            ngx_http_set_ctx(r, ctx, ngx_http_srcache_filter_module);

        } else {
            ngx_log_debug0(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                    "srcache_store skipped due to request header "
                    "\"Cache-Control: no-store\"");
        }

        return NGX_DECLINED;
    }

    if (conf->fetch_skip != NULL
            && ngx_http_complex_value(r, conf->fetch_skip, &skip) == NGX_OK
            && skip.len
            && (skip.len != 1 || skip.data[0] != '0'))
    {
        ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                "srcache_fetch skipped due to the true value fed into "
                "srcache_fetch_skip: \"%V\"", &skip);

        /* register a ctx to give a chance to srcache_store to run */

        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_srcache_filter_module));

        if (ctx == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_srcache_filter_module);

        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (ctx != NULL) {
        /*
        if (ctx->fetch_error) {
            return NGX_DECLINED;
        }
        */

        if (ctx->waiting_subrequest) {
            dd("waiting subrequest");
            return NGX_AGAIN;
        }

        if (ctx->request_done) {
            dd("request done");

            if (
#if defined(nginx_version) && nginx_version >= 8012
                ngx_http_post_request(r, NULL)
#else
                ngx_http_post_request(r)
#endif
                    != NGX_OK)
            {
                return NGX_ERROR;
            }

            if (!ctx->from_cache) {
                return NGX_DECLINED;
            }

            dd("sending header");

            if (ctx->body_from_cache && !(r->method & NGX_HTTP_HEAD)) {
                len = 0;

                for (cl = ctx->body_from_cache; cl->next; cl = cl->next) {
                    len += ngx_buf_size(cl->buf);
                }

                len += ngx_buf_size(cl->buf);

                cl->buf->last_buf = 1;

                r->headers_out.content_length_n = len;

                rc = ngx_http_srcache_next_header_filter(r);

                if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                rc = ngx_http_srcache_next_body_filter(r, ctx->body_from_cache);

                if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                dd("sent body from cache: %d", (int) rc);

            } else {
                r->headers_out.content_length_n = 0;

                rc = ngx_http_srcache_next_header_filter(r);

                if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                dd("sent header from cache: %d", (int) rc);

                dd("send last buf for the main request");

                cl = ngx_alloc_chain_link(r->pool);
                cl->buf = ngx_calloc_buf(r->pool);
                cl->buf->last_buf = 1;
                cl->next = NULL;

                rc = ngx_http_srcache_next_body_filter(r, cl);

                if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                dd("sent last buf from cache: %d", (int) rc);
            }

            dd("finalize from here...");
            ngx_http_finalize_request(r, NGX_OK);
            /* dd("r->main->count (post): %d", (int) r->main->count); */
            return NGX_DONE;
        }

    } else {
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_srcache_filter_module));

        if (ctx == NULL) {
            return NGX_HTTP_INTERNAL_SERVER_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_srcache_filter_module);
    }

    smcf = ngx_http_get_module_main_conf(r, ngx_http_srcache_filter_module);

    if (! smcf->postponed_to_access_phase_end) {
        ngx_http_core_main_conf_t       *cmcf;
        ngx_http_phase_handler_t         tmp;
        ngx_http_phase_handler_t        *ph;
        ngx_http_phase_handler_t        *cur_ph;
        ngx_http_phase_handler_t        *last_ph;

        smcf->postponed_to_access_phase_end = 1;

        cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

        ph = cmcf->phase_engine.handlers;
        cur_ph = &ph[r->phase_handler];

        /* we should skip the post_access phase handler here too */
        last_ph = &ph[cur_ph->next - 2];

        if (cur_ph < last_ph) {
            dd("swaping the contents of cur_ph and last_ph...");

            tmp = *cur_ph;

            memmove(cur_ph, cur_ph + 1,
                (last_ph - cur_ph) * sizeof (ngx_http_phase_handler_t));

            *last_ph = tmp;

            r->phase_handler--; /* redo the current ph */

            return NGX_DECLINED;
        }
    }

    if (conf->fetch == NULL) {
        dd("fetch is not defined");
        return NGX_DECLINED;
    }

    dd("running phase handler...");

    /* issue a subrequest to fetch cached stuff (if any) */

    rc = ngx_http_srcache_fetch_subrequest(r, conf, ctx);

    if (rc != NGX_OK) {
        return rc;
    }

    ctx->waiting_subrequest = 1;

    dd("quit");

    return NGX_AGAIN;
}


ngx_int_t
ngx_http_srcache_fetch_post_subrequest(ngx_http_request_t *r, void *data,
        ngx_int_t rc)
{
    ngx_http_srcache_ctx_t      *ctx = data;
    ngx_http_srcache_ctx_t      *pr_ctx;
    ngx_http_request_t          *pr;

    dd_enter();

    if (r != r->connection->data) {
        dd("waited: %d, rc: %d", (int) r->waited, (int) rc);
    }

    pr = r->parent;

    pr_ctx = ngx_http_get_module_ctx(pr, ngx_http_srcache_filter_module);
    if (pr_ctx == NULL) {
        return NGX_ERROR;
    }

    if (ctx && ctx->parsing_cached_headers) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                      "srcache_fetch: cache sent truncated status line "
                      "or headers");

        pr_ctx->from_cache = 0;
    }

    pr_ctx->waiting_subrequest = 0;
    pr_ctx->request_done = 1;

    return rc;
}


static ngx_int_t
ngx_http_srcache_fetch_subrequest(ngx_http_request_t *r,
        ngx_http_srcache_loc_conf_t *conf, ngx_http_srcache_ctx_t *ctx)
{
    ngx_http_srcache_ctx_t         *sr_ctx;
    ngx_http_post_subrequest_t     *psr;
    ngx_str_t                       args;
    ngx_uint_t                      flags = 0;
    ngx_http_request_t             *sr;
    ngx_int_t                       rc;

    ngx_http_srcache_parsed_request_t  *parsed_sr;

    dd_enter();

    parsed_sr = ngx_palloc(r->pool, sizeof(ngx_http_srcache_parsed_request_t));
    if (parsed_sr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    parsed_sr->method      = conf->fetch->method;
    parsed_sr->method_name = conf->fetch->method_name;

    parsed_sr->request_body = NULL;
    parsed_sr->content_length_n = -1;

    if (ngx_http_complex_value(r, &conf->fetch->location,
                &parsed_sr->location) != NGX_OK)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (parsed_sr->location.len == 0) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_complex_value(r, &conf->fetch->args, &parsed_sr->args)
            != NGX_OK)
    {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    args.data = NULL;
    args.len = 0;

    if (ngx_http_parse_unsafe_uri(r, &parsed_sr->location, &args, &flags)
            != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (args.len > 0 && parsed_sr->args.len == 0) {
        parsed_sr->args = args;
    }

    sr_ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_srcache_ctx_t));
    if (sr_ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    sr_ctx->in_fetch_subrequest = 1;

    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    psr->handler = ngx_http_srcache_fetch_post_subrequest;
    psr->data = sr_ctx;

    dd("firing the fetch subrequest");

    dd("fetch location: %.*s", (int) parsed_sr->location.len,
            parsed_sr->location.data);

    dd("fetch args: %.*s", (int) parsed_sr->args.len,
            parsed_sr->args.data);

    rc = ngx_http_subrequest(r, &parsed_sr->location, &parsed_sr->args,
            &sr, psr, flags);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_http_srcache_adjust_subrequest(sr, parsed_sr);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(sr, sr_ctx, ngx_http_srcache_filter_module);

    ctx->fetch_sr = sr;

    return NGX_OK;
}




