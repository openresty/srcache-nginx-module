# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * (5 * blocks());

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

