# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

no_shuffle();

run_tests();

__DATA__

=== TEST 1: flush all
--- config
    location /flush {
        set $memc_cmd 'flush_all';
        memc_pass 127.0.0.1:11984;
    }
--- request
GET /flush
--- response_body eval: "OK\r\n"



=== TEST 1: basic fetch
--- config
    location /foo {
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo hello;
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:11984;
    }
--- request
GET /foo
--- response_body
hello


=== TEST 1: basic fetch
--- config
    location /foo {
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo world;
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:11984;
    }
--- request
GET /foo
--- response_body
hello

