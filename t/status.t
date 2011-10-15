# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

plan tests => repeat_each() * 37;

$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;

no_long_string();

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
!Foo-Bar
--- request
GET /flush
--- response_body eval: "OK\r\n"



=== TEST 2: basic fetch (http 1.0)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo hello;
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo HTTP/1.0
--- response_headers
Content-Type: text/css
Content-Length: 6
--- response_body
hello



=== TEST 3: inspect the cached item
--- config
    location /memc {
        set $memc_key "/foo";
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /memc
--- response_headers
Content-Type: text/plain
Content-Length: 49
!Set-Cookie
!Proxy-Authenticate
--- response_body eval
"HTTP/1.1 200 OK\r
Content-Type: text/css\r
\r
hello
"



=== TEST 4: flush all
--- config
    location /flush {
        set $memc_cmd 'flush_all';
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- response_headers
Content-Type: text/plain
Content-Length: 4
!Foo-Bar
--- request
GET /flush
--- response_body eval: "OK\r\n"



=== TEST 5: basic fetch (cache 500 404 200 statuses)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;
        srcache_store_statuses 500 200 404;

        content_by_lua '
            ngx.exit(404)
        ';
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo HTTP/1.0
--- response_headers
Content-Type: text/html
--- response_body_like: 404 Not Found
--- error_code: 404



=== TEST 6: inspect the cached item
--- config
    location /memc {
        set $memc_key "/foo";
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /memc
--- response_headers
Content-Type: text/plain
--- response_body_like
^HTTP/1\.1 404 Not Found\r
Content-Type: text/html\r
\r
.*?404 Not Found.*



=== TEST 7: flush all
--- config
    location /flush {
        set $memc_cmd 'flush_all';
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- response_headers
Content-Type: text/plain
Content-Length: 4
!Foo-Bar
--- request
GET /flush
--- response_body eval: "OK\r\n"



=== TEST 8: basic fetch (404 not listed in store_statuses)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;
        srcache_store_statuses 500 200 410;

        content_by_lua '
            ngx.exit(404)
        ';
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo HTTP/1.0
--- response_headers
Content-Type: text/html
--- response_body_like: 404 Not Found
--- error_code: 404



=== TEST 9: inspect the cached item
--- config
    location /memc {
        set $memc_key "/foo";
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /memc
--- response_headers
Content-Type: text/html
--- response_body_like: 404 Not Found
--- error_code: 404

