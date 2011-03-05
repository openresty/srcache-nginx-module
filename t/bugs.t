# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * (2 * blocks() + 2);

$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;

run_tests();

__DATA__

=== TEST 1: basic fetch
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc;
        srcache_store PUT /memc;

        echo hello;
    }

    location /memc {
        internal;

        set $memc_key 'foooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo';
        set $memc_exptime 300;
        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo
--- response_headers
Content-Type: text/css
Content-Length:
--- response_body
hello
--- timeout: 15

