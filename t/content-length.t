# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * (blocks() * 5 - 4);

$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;

#master_on();
no_shuffle();

run_tests();

__DATA__

=== TEST 1: flush all
--- config
    location /flush {
        set $memc_cmd 'flush_all';
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- response_headers
Content-Type: text/plain
Content-Length: 4
--- request
GET /flush
--- response_body eval: "OK\r\n"
--- no_error_log
[error]



=== TEST 2: basic fetch (cache miss)
# IMPORTANT: nginx 1.29.6 changed the default of "proxy_http_version" from
# "1.0" to "1.1" (CHANGES, 10 Mar 2026: "now ngx_http_proxy_module supports
# keepalive by default; the default value for proxy_http_version is 1.1").
# This test runs under that new default (branch ngx-1.29.8).

# /gate advertises Content-Length: 10 but only writes 6 bytes ("hello\n"),
# and modern nginx/openresty keeps the upstream connection alive afterwards.
# proxy_pass therefore blocks waiting for the missing 4 bytes, so the test
# client never sees EOF. --- abort + --- timeout tell Test::Nginx::Socket
# that the client timeout is expected; the real assertion is the
# --- no_error_log check below (srcache_store must skip a truncated body).
--- config
    location /foo {
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        proxy_pass http://127.0.0.1:$server_port/gate;
    }

    location = /gate {
        default_type text/css;
        content_by_lua '
            ngx.header.content_length = 10
            ngx.say("hello")
        ';
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo
--- response_headers
Content-Type: text/css
Content-Length: 10
--- ignore_response
--- abort
--- timeout: 1
--- no_error_log
srcache_store: subrequest returned status



=== TEST 3: basic fetch (cache miss)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo world;
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo
--- response_headers
Content-Type: text/css
!Content-Length
--- response_body
world
--- no_error_log
[error]
