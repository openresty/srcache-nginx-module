#define DDEBUG 1
#include "ddebug.h"

/*
 * Copyright (C) Yichun Zhang (agentzh)
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include "ngx_http_srcache_filter_module.h"
#include "ngx_http_srcache_util.h"


ngx_flag_t  ngx_http_srcache_used = 0;

static void *ngx_http_srcache_create_conf(ngx_conf_t *cf);
static char *ngx_http_srcache_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_srcache_filter_init(ngx_conf_t *cf);
static char *ngx_http_srcache_conf_set_request(ngx_conf_t *cf,
        ngx_command_t *cmd, void *conf);

static ngx_int_t ngx_http_srcache_handler(ngx_http_request_t *r);
static ngx_int_t ngx_http_srcache_post_fetch_subrequest(ngx_http_request_t *r,
        void *data, ngx_int_t rc);


static ngx_command_t  ngx_http_srcache_commands[] = {

    { ngx_string("srcache_buffer"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
          |NGX_CONF_TAKE1,
      ngx_conf_set_size_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_srcache_conf_t, buf_size),
      NULL },

    { ngx_string("srcache_fetch"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
          |NGX_CONF_TAKE23,
      ngx_http_srcache_conf_set_request,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_srcache_conf_t, fetch),
      NULL },

    { ngx_string("srcache_store"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
          |NGX_CONF_TAKE23,
      ngx_http_srcache_conf_set_request,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_srcache_conf_t, store),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_srcache_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_srcache_filter_init,         /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_srcache_create_conf,         /* create location configuration */
    ngx_http_srcache_merge_conf           /* merge location configuration */
};


ngx_module_t  ngx_http_srcache_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_srcache_filter_module_ctx,  /* module context */
    ngx_http_srcache_commands,            /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


static ngx_int_t
ngx_http_srcache_header_filter(ngx_http_request_t *r)
{
    ngx_http_srcache_ctx_t   *ctx, *parent_ctx;
    ngx_http_srcache_conf_t  *conf;

    dd_enter();

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (ctx == NULL || ctx->from_cache) {
        dd("bypass");
        return ngx_http_next_header_filter(r);
    }

    if (ctx->in_fetch_subrequest) {
        dd("in fetch subrequest");

        parent_ctx = ngx_http_get_module_ctx(r->parent,
                ngx_http_srcache_filter_module);

        if (parent_ctx == NULL) {
            dd("parent ctx is null");

            ctx->ignore_body = 1;

            return NGX_ERROR;
        }

        if (r->headers_out.status != NGX_HTTP_OK
                && r->headers_out.status != NGX_HTTP_CREATED)
        {
            ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                    "srcache: subrequest returned status %d", r->headers_out.status);

            ctx->ignore_body = 1;

            parent_ctx->waiting_subrequest = 0;

            return NGX_OK;
        }

        dd("srcache's subrequest succeeds");

        /* r->filter_need_in_memory = 1; */

        parent_ctx->from_cache = 1;

        ctx->parsing_cached_headers = 1;

        return NGX_OK;
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_srcache_filter_module);

    if (conf->store == NULL) {
        return ngx_http_next_header_filter(r);
    }

    dd("try to save the response header");

    if (r->headers_out.status != NGX_HTTP_OK
            && r->headers_out.status != NGX_HTTP_CREATED)
    {
        return ngx_http_next_header_filter(r);
    }

    if (r == r->main) {
        /* being the main request */
    } else {
        /* being a subrequest */
    }

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_srcache_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_srcache_ctx_t   *ctx, *parent_ctx;
    /* ngx_http_srcache_conf_t  *conf; */
    ngx_int_t                 rc;

    dd_enter();

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (ctx == NULL || ctx->from_cache) {
        dd("bypass");
        return ngx_http_next_body_filter(r, in);
    }

    if (ctx->ignore_body) {
        /* also for ctx->in_store_sebrequest == 1 */
        ngx_http_srcache_discard_bufs(r->pool, in);
        return NGX_OK;
    }

    if (ctx->in_fetch_subrequest) {
        if (ctx->parsing_cached_headers) {
            /* parse the cached response's headers and
             * set r->parent->headers_out */

            dd("restore parent request header");

            r->parent->headers_out.status = NGX_HTTP_OK;

            ctx->parsing_cached_headers = 0;
        }

        dd("save the cached response body for parent");

        parent_ctx = ngx_http_get_module_ctx(r->parent,
                ngx_http_srcache_filter_module);
        if (parent_ctx == NULL) {
            return NGX_ERROR;
        }

        rc = ngx_http_srcache_add_copy_chain(r->pool, &parent_ctx->body_from_cache, in);
        if (rc != NGX_OK) {
            ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                    "upstream bufs contain non-memory ones");

            return NGX_ERROR;
        }

        ngx_http_srcache_discard_bufs(r->pool, in);

        return NGX_OK;
    }

    /* TODO */
    return NGX_OK;
}


static ngx_int_t
ngx_http_srcache_filter_init(ngx_conf_t *cf)
{
    if (ngx_http_srcache_used) {
        ngx_http_handler_pt             *h;
        ngx_http_core_main_conf_t       *cmcf;

        /* register our output filters */

        ngx_http_next_header_filter = ngx_http_top_header_filter;
        ngx_http_top_header_filter = ngx_http_srcache_header_filter;

        ngx_http_next_body_filter = ngx_http_top_body_filter;
        ngx_http_top_body_filter = ngx_http_srcache_body_filter;

        /* register our rewrite-phase handler */

        cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

        h = ngx_array_push(&cmcf->phases[NGX_HTTP_REWRITE_PHASE].handlers);
        if (h == NULL) {
            return NGX_ERROR;
        }

        *h = ngx_http_srcache_handler;
    }

    return NGX_OK;
}


static void *
ngx_http_srcache_create_conf(ngx_conf_t *cf)
{
    ngx_http_srcache_conf_t  *conf;

    conf = ngx_palloc(cf->pool, sizeof(ngx_http_srcache_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    conf->fetch = NGX_CONF_UNSET_PTR;
    conf->store = NGX_CONF_UNSET_PTR;

    conf->buf_size = NGX_CONF_UNSET_SIZE;

    return conf;
}


static char *
ngx_http_srcache_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_srcache_conf_t *prev = parent;
    ngx_http_srcache_conf_t *conf = child;

    ngx_conf_merge_ptr_value(conf->fetch, prev->fetch, NULL);
    ngx_conf_merge_ptr_value(conf->store, prev->store, NULL);

    ngx_conf_merge_size_value(conf->buf_size, prev->buf_size,
            (size_t) ngx_pagesize);

    return NGX_CONF_OK;
}


static char *
ngx_http_srcache_conf_set_request(ngx_conf_t *cf, ngx_command_t *cmd,
        void *conf)
{
    char  *p = conf;

    ngx_http_srcache_request_t      **rpp;
    ngx_http_srcache_request_t       *rp;
    ngx_str_t                        *value;
    ngx_str_t                        *method_name;
    ngx_http_compile_complex_value_t  ccv;

    rpp = (ngx_http_srcache_request_t **) (p + cmd->offset);

    if (*rpp != NGX_CONF_UNSET_PTR) {
        return "is duplicate";
    }

    ngx_http_srcache_used = 1;

    value = cf->args->elts;

    *rpp = ngx_pcalloc(cf->pool, sizeof(ngx_http_srcache_request_t));
    if (*rpp == NULL) {
        return NGX_CONF_ERROR;
    }

    rp = *rpp;

    method_name = &value[1];

    rp->method = ngx_http_srcache_parse_method_name(&method_name);

    if (rp->method == NGX_HTTP_UNKNOWN) {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                "%V specifies bad HTTP method %V",
                &cmd->name, method_name);

        return NGX_CONF_ERROR;
    }

    rp->method_name = *method_name;

    /* compile the location arg */

    if (value[2].len == 0) {
        ngx_memzero(&rp->location, sizeof(ngx_http_complex_value_t));

    } else {
        ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

        ccv.cf = cf;
        ccv.value = &value[2];
        ccv.complex_value = &rp->location;

        if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
            return NGX_CONF_ERROR;
        }
    }

    if (cf->args->nelts == 2 + 1) {
        return NGX_CONF_OK;
    }

    /* compile the args arg */

    if (value[3].len == 0) {
        ngx_memzero(&rp->location, sizeof(ngx_http_complex_value_t));
        return NGX_CONF_OK;
    }

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[3];
    ccv.complex_value = &rp->args;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_srcache_handler(ngx_http_request_t *r)
{
    ngx_int_t                       rc;
    ngx_http_srcache_conf_t        *conf;
    ngx_http_srcache_ctx_t         *ctx;
    ngx_http_srcache_ctx_t         *sr_ctx;
    ngx_http_post_subrequest_t     *psr;
    ngx_str_t                       args;
    ngx_uint_t                      flags = 0;
    ngx_http_request_t             *sr;

    ngx_http_srcache_parsed_request_t  *parsed_sr;

    dd_enter();

    conf = ngx_http_get_module_loc_conf(r, ngx_http_srcache_filter_module);

    if (conf->fetch == NULL && conf->store == NULL) {
        dd("bypass");
        return NGX_DECLINED;
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (ctx != NULL) {
        if (ctx->waiting_subrequest) {
            dd("waiting subrequest");
            return NGX_AGAIN;
        }

        if (ctx->in_fetch_subrequest || ctx->in_store_subrequest) {
            dd("in fetch/store subrequest");
            return NGX_DECLINED;
        }

        if (ctx->request_done) {
            dd("request done");

            rc = ngx_http_send_header(r);
            if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                return rc;
            }

            dd("sent header from cache: %d", (int) rc);

            if (ctx->body_from_cache) {
                rc = ngx_http_output_filter(r, ctx->body_from_cache);

                if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                dd("sent body from cache: %d", (int) rc);
            }

            if (r == r->main) {
                dd("send last buf for the main request");
                rc = ngx_http_send_special(r, NGX_HTTP_LAST);

                if (rc == NGX_ERROR || rc >= NGX_HTTP_SPECIAL_RESPONSE) {
                    return rc;
                }

                dd("sent last buf from cache: %d", (int) rc);
            }

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

    if ( ! ctx->postponed_to_phase_end ) {
        ngx_http_core_main_conf_t       *cmcf;
        ngx_http_phase_handler_t        tmp;
        ngx_http_phase_handler_t        *ph;
        ngx_http_phase_handler_t        *cur_ph;
        ngx_http_phase_handler_t        *last_ph;

        ctx->postponed_to_phase_end = 1;

        cmcf = ngx_http_get_module_main_conf(r, ngx_http_core_module);

        ph = cmcf->phase_engine.handlers;
        cur_ph = &ph[r->phase_handler];
        last_ph = &ph[cur_ph->next - 1];

        if (cur_ph < last_ph) {
            dd("swaping the contents of cur_ph and last_ph...");
            tmp      = *cur_ph;
            *cur_ph  = *last_ph;
            *last_ph = tmp;

            r->phase_handler--; /* redo the current ph */

            return NGX_DECLINED;
        }
    }

    if (conf->fetch == NULL) {
        return NGX_DECLINED;
    }

    dd("running phase handler...");

    /* issue a subrequest to fetch cached stuff (if any) */

    parsed_sr = ngx_palloc(r->pool, sizeof(ngx_http_srcache_parsed_request_t));
    if (parsed_sr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    parsed_sr->method      = conf->fetch->method;
    parsed_sr->method_name = conf->fetch->method_name;

    parsed_sr->request_body = NULL;
    parsed_sr->content_length_n = -1;

    if (ngx_http_complex_value(r, &conf->fetch->location, &parsed_sr->location) != NGX_OK) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (parsed_sr->location.len == 0) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    if (ngx_http_complex_value(r, &conf->fetch->args, &parsed_sr->args) != NGX_OK) {
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

    psr = ngx_palloc(r->pool, sizeof(ngx_http_post_subrequest_t));
    if (psr == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    psr->handler = ngx_http_srcache_post_fetch_subrequest;
    psr->data = ctx;

    dd("firing the fetch subrequest");

    dd("fetch location: %.*s", (int) parsed_sr->location.len,
            parsed_sr->location.data);

    dd("fetch args: %.*s", (int) parsed_sr->args.len,
            parsed_sr->args.data);

    rc = ngx_http_subrequest(r, &parsed_sr->location, &parsed_sr->args, &sr, psr, flags);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    rc = ngx_http_srcache_adjust_subrequest(sr, parsed_sr);

    if (rc != NGX_OK) {
        return NGX_ERROR;
    }

    ctx->waiting_subrequest = 1;

    sr_ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_srcache_ctx_t));
    if (sr_ctx == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    sr_ctx->in_fetch_subrequest = 1;

    ngx_http_set_ctx(sr, sr_ctx, ngx_http_srcache_filter_module);

    dd("quit");

    return NGX_AGAIN;
}


static ngx_int_t
ngx_http_srcache_post_fetch_subrequest(ngx_http_request_t *r, void *data, ngx_int_t rc)
{
    ngx_http_srcache_ctx_t     *parent_ctx = data;

    dd_enter();

    parent_ctx->waiting_subrequest = 0;
    parent_ctx->request_done = 1;

    return rc;
}

