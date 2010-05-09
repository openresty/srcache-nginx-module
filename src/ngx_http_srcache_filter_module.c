
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

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (ctx == NULL || ctx->from_cache) {
        return ngx_http_next_header_filter(r);
    }

    if (ctx->in_fetch_subrequest) {
        parent_ctx = ngx_http_get_module_ctx(r->parent,
                ngx_http_srcache_filter_module);

        if (parent_ctx == NULL) {
            ctx->ignore_body = 1;

            return NGX_ERROR;
        }

        if (r->headers_out.status != NGX_HTTP_OK
                || r->headers_out.status != NGX_HTTP_CREATED) {
            ctx->ignore_body = 1;

            parent_ctx->waiting_subrequest = 0;

            return NGX_OK;
        }

        /* srache's subrequest succeeds */

        r->filter_need_in_memory = 1;

        parent_ctx->from_cache = 1;

        ctx->parsing_cached_headers = 1;

        return NGX_OK;
    }

    if (r->headers_out.status != NGX_HTTP_OK
            && r->headers_out.status != NGX_HTTP_CREATED)
    {
        return ngx_http_next_header_filter(r);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_srcache_filter_module);

    if (conf->store == NULL) {
        return ngx_http_next_header_filter(r);
    }

    /* save the response header */

    if (r == r->main) {
        /* being the main request */
    } else {
        /* being a subrequest */
    }

    return NGX_OK;
}


static ngx_int_t
ngx_http_srcache_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_http_srcache_ctx_t   *ctx; /*, *parent_ctx; */
    /* ngx_http_srcache_conf_t  *conf; */

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (ctx == NULL || ctx->from_cache) {
        return ngx_http_next_body_filter(r, in);
    }

    if (ctx->ignore_body) {
        ngx_http_srcache_discard_bufs(r->pool, in);
        return NGX_OK;
    }

    if (ctx->in_fetch_subrequest) {
        if (ctx->parsing_cached_headers) {
            /* parse the cached response's headers and
             * set r->parent->headers_out */

            (void) ngx_http_send_header(r->parent);
        }

        /* pass along the cached response body */

        return ngx_http_next_body_filter(r->parent, in);
    }

    /* TODO */
    return NGX_OK;
}


static ngx_int_t
ngx_http_srcache_filter_init(ngx_conf_t *cf)
{
    if (ngx_http_srcache_used) {
        ngx_http_next_header_filter = ngx_http_top_header_filter;
        ngx_http_top_header_filter = ngx_http_srcache_header_filter;

        ngx_http_next_body_filter = ngx_http_top_body_filter;
        ngx_http_top_body_filter = ngx_http_srcache_body_filter;
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

    value = cf->args->elts;

    *rpp = ngx_pcalloc(cf->pool, sizeof(ngx_http_srcache_request_t));
    if (*rpp == NULL) {
        return NGX_CONF_ERROR;
    }

    rp = *rpp;

    method_name = &value[1];

    rp->method = ngx_http_srcache_parse_method_name(&method_name);
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

