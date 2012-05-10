# vi:filetype=

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * (4 * blocks());

$ENV{TEST_NGINX_MEMCACHED_PORT} ||= 11211;

no_shuffle();

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



=== TEST 2: internal redirect in fetch subrequest
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /fetch;

        echo hello;
    }
    location /fetch {
        echo_exec /bar;
    }
    location /bar {
        default_type 'text/css';
        echo "HTTP/1.1 200 OK\r\nContent-Type: text/css\r\n\r\nbar";
    }
--- request
GET /foo
--- response_headers
Content-Type: text/css
Content-Length: 4
--- response_body
bar



=== TEST 3: flush all
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



=== TEST 4: internal redirect in main request (no caching happens) (cache miss)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo_exec /bar;
    }

    location /bar {
        default_type text/javascript;
        echo hello;
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
Content-Type: text/javascript
! Content-Length
--- response_body
hello



=== TEST 5: internal redirect happends in the main request (cache miss as well)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        #srcache_store PUT /memc $uri;

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



=== TEST 6: flush all
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



=== TEST 7: internal redirect in store subrequest
--- config
    location /foo {
        default_type text/css;
        srcache_store GET /store;

        echo blah;
    }
    location /store {
        echo_exec /set-value;
    }
    location /set-value {
        set $memc_key foo;
        set $memc_value "HTTP/1.0 201 Created\r\nContent-Type: text/blah\r\n\r\nbar";
        set $memc_cmd set;

        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo
--- response_headers
Content-Type: text/css
!Content-Length
--- response_body
blah



=== TEST 8: internal redirect in store subrequest (check if value has been stored)
--- config
    location /foo {
        default_type text/css;
        srcache_fetch GET /fetch;

        echo blah;
    }
    location /fetch {
        set $memc_key foo;
        set $memc_cmd get;

        memc_pass 127.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /foo
--- response_headers
Content-Type: text/blah
Content-Length: 3
--- response_body chop
bar
--- error_code: 201



=== TEST 9: skipped in subrequests
--- config
    location /sub {
        default_type text/css;
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo hello;
    }

    location /main {
        echo_location /sub;
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 128.0.0.1:$TEST_NGINX_MEMCACHED_PORT;
    }
--- request
GET /main
--- response_headers
Content-Type: text/plain
Content-Length:
--- response_body
hello



=== TEST 10: multi-buffer response resulted in incorrect request length header
--- config
    location /foo {
        default_type text/css;
        srcache_store POST /store;

        echo hello;
        echo world;
    }

    location /store {
        content_by_lua '
            ngx.log(ngx.WARN, "srcache_store: request Content-Length: ", ngx.var.http_content_length)
            -- local body = ngx.req.get_body_data()
            -- ngx.log(ngx.WARN, "srcache_store: request body len: ", #body)
        ';
    }
--- request
GET /foo
--- response_headers
!Content-Length
--- response_body
hello
world
--- error_log
srcache_store: request Content-Length: 55

