# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

repeat_each(2);

plan tests => repeat_each() * 4 * blocks();

$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;

#master_on();
no_shuffle();

run_tests();

__DATA__

=== TEST 1: basic fetch (cache miss)
--- config
    error_page   500 502 503 504  /50x.html;
    location = /50x.html {
       root   html;
    }

    location = /foo {
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        proxy_pass http://127.0.0.1:$server_port/hello;
    }

    location = /hello {
        echo hello;
        default_type text/css;
    }

    memc_connect_timeout 1ms;
    location = /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass www.google.com:1234;
    }
--- user_files
>>> 50x.html
bad bad
--- request
GET /foo
--- response_headers
Content-Type: text/css
!Content-Length
--- response_body
hello

