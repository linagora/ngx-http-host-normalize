/*
 * ngx_http_host_normalize_module.c
 *
 * Nginx module to normalize the Host header when an absolute URI is used
 * in the request line, as per RFC 9112 Section 3.2.2.
 *
 * This fixes a security issue where an attacker can bypass access controls
 * by sending a request with an absolute URI pointing to one host while
 * setting the Host header to another host.
 *
 * Example attack:
 *   GET http://protected.example.com/ HTTP/1.1
 *   Host: public.example.com
 *
 * Without this module, backends (FastCGI, uWSGI, SCGI) receive HTTP_HOST
 * as "public.example.com" while the request is actually routed to
 * "protected.example.com".
 *
 * Copyright (c) 2026 LemonLDAP::NG Team
 * Licensed under the BSD 2-Clause License
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static ngx_int_t ngx_http_host_normalize_init(ngx_conf_t *cf);


static ngx_http_module_t ngx_http_host_normalize_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_host_normalize_init,          /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t ngx_http_host_normalize_module = {
    NGX_MODULE_V1,
    &ngx_http_host_normalize_module_ctx,   /* module context */
    NULL,                                  /* module directives */
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


/*
 * Handler that normalizes the Host header when an absolute URI was used.
 *
 * RFC 9112 Section 3.2.2 states:
 * "When an origin server receives a request with an absolute-form of
 *  request-target, the origin server MUST ignore the received Host header
 *  field (if any) and instead use the host information of the request-target."
 *
 * This handler ensures that HTTP_HOST passed to backends matches the
 * host from the request-target (absolute URI) rather than the potentially
 * spoofed Host header.
 */
static ngx_int_t
ngx_http_host_normalize_handler(ngx_http_request_t *r)
{
    ngx_table_elt_t  *h;
    u_char           *lowcase;

    /*
     * r->headers_in.server contains the host extracted from an absolute URI
     * in the request line (e.g., "GET http://example.com/ HTTP/1.1").
     * If this is set and differs from the Host header, we normalize it.
     */
    if (r->headers_in.server.len == 0) {
        /* No absolute URI was used, nothing to normalize */
        return NGX_DECLINED;
    }

    h = r->headers_in.host;
    if (h == NULL) {
        /* No Host header present, nothing to normalize */
        return NGX_DECLINED;
    }

    /*
     * Check if Host header differs from the request-target host.
     * We need to handle the case where one includes a port and the other doesn't.
     * For simplicity, we always normalize when an absolute URI is present.
     */

    /* Allocate memory for the lowcase version */
    lowcase = ngx_pnalloc(r->pool, r->headers_in.server.len);
    if (lowcase == NULL) {
        return NGX_ERROR;
    }

    /* Convert to lowercase for consistent comparison */
    ngx_strlow(lowcase, r->headers_in.server.data, r->headers_in.server.len);

    /* Replace the Host header value with the host from the absolute URI */
    h->value.len = r->headers_in.server.len;
    h->value.data = r->headers_in.server.data;
    h->lowcase_key = lowcase;

    /* Also update the host field that nginx uses internally */
    r->headers_in.host->value = r->headers_in.server;

    ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
                   "host_normalize: normalized Host header to \"%V\"",
                   &r->headers_in.server);

    return NGX_DECLINED;
}


/*
 * Module initialization: register our handler in the POST_READ phase.
 * This phase runs after request headers are read but before any
 * access control or content handling.
 */
static ngx_int_t
ngx_http_host_normalize_init(ngx_conf_t *cf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_POST_READ_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_host_normalize_handler;

    return NGX_OK;
}
