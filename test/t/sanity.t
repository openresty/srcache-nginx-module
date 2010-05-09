# vi:filetype=perl

use lib 'lib';
use Test::Nginx::Socket;

#repeat_each(2);

plan tests => repeat_each() * 2 * blocks();

run_tests();

__DATA__

=== TEST 1: simple fetch
--- config
    location /main {
        echo_location /foo;
        echo_location /foo;
    }

    location /foo {
        srcache_fetch GET /memc $uri;
        srcache_store PUT /memc $uri;

        echo $echo_incr;
    }

    location /memc {
        internal;

        set $memc_key $query_string;
        set $memc_exptime 300;
        memc_pass 127.0.0.1:11984;
    }
--- request
GET /main
--- response_body
1
1

