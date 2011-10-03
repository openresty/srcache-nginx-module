#ifndef DDEBUG
#define DDEBUG 0
#endif
#include "ddebug.h"


#include "ngx_http_srcache_var.h"


static ngx_int_t ngx_http_srcache_expire_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data);


static ngx_http_variable_t ngx_http_srcache_variables[] = {

    { ngx_string("srcache_expire"), NULL,
      ngx_http_srcache_expire_variable, 0,
      NGX_HTTP_VAR_NOCACHEABLE, 0 },

    { ngx_null_string, NULL, NULL, 0, 0, 0 }
};


static ngx_int_t
ngx_http_srcache_expire_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_srcache_ctx_t       *ctx;
    u_char                       *p;
    time_t                        expire;
    ngx_http_srcache_loc_conf_t  *conf;

    conf = ngx_http_get_module_loc_conf(r, ngx_http_srcache_filter_module);

    v->valid = 1;
    v->no_cacheable = 1;
    v->not_found = 0;

    ctx = ngx_http_get_module_ctx(r, ngx_http_srcache_filter_module);

    if (!ctx || !ctx->store_response) {
        v->not_found = 1;
        return NGX_OK;
    }

    if (ctx->valid_sec == 0) {
        expire = conf->default_expire;

    } else {
        expire = ctx->valid_sec - ngx_time();
    }

    if (conf->max_expire > 0 && expire > conf->max_expire) {
        expire = conf->max_expire;
    }

    p = ngx_palloc(r->pool, NGX_TIME_T_LEN);
    if (p == NULL) {
        return NGX_ERROR;
    }

    v->data = p;
    p =  ngx_sprintf(p, "%T", expire);
    v->len = p - v->data;

    return NGX_OK;
}


ngx_int_t
ngx_http_srcache_add_variables(ngx_conf_t *cf)
{
    ngx_http_variable_t *var, *v;

    for (v = ngx_http_srcache_variables; v->name.len; v++) {
        var = ngx_http_add_variable(cf, &v->name, v->flags);
        if (var == NULL) {
            return NGX_ERROR;
        }

        var->get_handler = v->get_handler;
        var->data = v->data;
    }

    return NGX_OK;
}

