Name
====

**ngx_srcache** - Transparent subrequest-based caching layout for arbitrary nginx locations

*This module is not distributed with the Nginx source.* See [the installation instructions](http://wiki.nginx.org/HttpSRCacheModule#Installation).

Status
======

This module is production ready.

Version
=======

This document describes srcache-nginx-module [v0.12rc6](https://github.com/agentzh/srcache-nginx-module/downloads) released on 30 June 2011.

Synopsis
========


    upstream my_memcached {
        server 10.62.136.7:11211;
        keepalive 512 single; # this requires the ngx_http_upstream_keepalive module
    }

    location = /memc {
        internal;

        memc_connect_timeout 100ms;
        memc_send_timeout 100ms;
        memc_read_timeout 100ms;

        set $memc_key $query_string;
        set $memc_exptime 300;

        memc_pass my_memcached;
    }

    location /foo {
        charset utf-8; # or some other encoding
        default_type text/plain; # or some other MIME type
  
        set $key $uri$args;
        srcache_fetch GET /memc $key;
        srcache_store PUT /memc $key;
  
        # proxy_pass/fastcgi_pass/drizzle_pass/echo/etc...
        # or even static files on the disk
    }



    location = /memc2 {
        internal;

        memc_connect_timeout 100ms;
        memc_send_timeout 100ms;
        memc_read_timeout 100ms;

        set_unescape_uri $memc_key $arg_key;
        set $memc_exptime $arg_exptime;

        memc_pass unix:/tmp/memcached.sock;
    }

    location /bar {
        charset utf-8; # or some other encoding
        default_type text/plain; # or some other MIME type
  
        set_escape_uri $key $uri$args;
        srcache_fetch GET /memc2 key=$key;
        srcache_store PUT /memc2 key=$key&exptime=3600;
  
        # proxy_pass/fastcgi_pass/drizzle_pass/echo/etc...
        # or even static files on the disk
    }


Description
===========

This module provides a transparent caching layer for arbitrary nginx locations (like those use an upstream or even serve static disk files).

Usually, [HttpMemcModule](http://wiki.nginx.org/HttpMemcModule) is used together with this module to provide a concrete caching storage backend. But technically, any modules that provide a REST interface can be used as the fetching and storage subrequests used by this module.

For main requests, the [srcache_fetch](http://wiki.nginx.org/HttpSRCacheModule#srcache_fetch) directive works at the end of the access phase, so the [standard access module](http://wiki.nginx.org/HttpAccessModule)'s [allow](http://wiki.nginx.org/HttpAccessModule#allow) and [deny](http://wiki.nginx.org/HttpAccessModule#deny) direcives run *before* ours, which is usually the desired behavior for security reasons.

Subrequest caching
------------------

For subrequests, we explicitly disallow the use of this module because it's too difficult to get right. There used to be an implementation but it was buggy and I finally gave up fixing it and abandoned it.

However, if you're using [HttpLuaModule](http://wiki.nginx.org/HttpLuaModule), it's easy to do subrequest caching in Lua all by yourself. That is, first issue a subrequest to an [HttpMemcModule](http://wiki.nginx.org/HttpMemcModule) location to do an explicit cache lookup, if cache hit, just use the cached data returned; otherwise, fall back to the true backend, and finally do a cache insertion to feed the data into the cache.

Using this module for main request caching and Lua for subrequest caching is the approach that we're taking in our business. This hybrid solution works great in production.

Distributed Memcached Caching
-----------------------------

Here is a simple example demonstrating a distributed memcached caching mechanism built atop this module. Suppose we do have three different memcacached nodes and we use simple modulo to hash our keys.


    http {
        upstream moon {
            server 10.62.136.54:11211;
            server unix:/tmp/memcached.sock backup;
        }

        upstream earth {
            server 10.62.136.55:11211;
        }

        upstream sun {
            server 10.62.136.56:11211;
        }

        upstream_list universe moon earth sun;

        server {
            memc_connect_timeout 100ms;
            memc_send_timeout 100ms;
            memc_read_timeout 100ms;

            location = /memc {
                internal;

                set $memc_key $query_string;
                set_hashed_upstream $backend universe $memc_key;
                set $memc_exptime 3600; # in seconds
                memc_pass $backend;
            }

            location / {
                set $key $uri;
                srcache_fetch GET /memc $key;
                srcache_store PUT /memc $key;

                # proxy_pass/fastcgi_pass/content_by_lua/drizzle_pass/...
            }
        }
    }

Here's what is going on in the sample above:
1. We first define three upstreams, `moon`, `earth`, and `sun`. These are our three memcached servers.
1. And then we group them together as an upstream list entity named `universe` with the `upstream_list` directive provided by [HttpSetMiscModule](http://wiki.nginx.org/HttpSetMiscModule).
1. After that, we define an internal location named `/memc` for talking to the memcached cluster.
1. In this `/memc` location, we first set the `$memc_key` variable with the query string (`$args`), and then use the [set_hashed_upstream](http://wiki.nginx.org/HttpSetMiscModule#set_hashed_upstream) directive to hash our [$memc_key](http://wiki.nginx.org/HttpMemcModule#.24memc_key) over the upsteam list `universe`, so as to obtain a concrete upstream name to be assigned to the variable `$backend`.
1. We pass this `$backend` variable into the [memc_pass](http://wiki.nginx.org/HttpMemcModule#memc_pass) directive. The `$backend` variable can hold a value among `moon`, `earth`, and `sun`.
1. Also, we define the memcached caching expiration time to be 3600 seconds (i.e., an hour) by overriding the [$memc_exptime](http://wiki.nginx.org/HttpMemcModule#.24memc_exptime) variable.
1. In our main public location `/`, we configure the `$uri` variable as our cache key, and then configure [srcache_fetch](http://wiki.nginx.org/HttpSRCacheModule#srcache_fetch) for cache lookups and [srcache_store](http://wiki.nginx.org/HttpSRCacheModule#srcache_store) for cache updates. We're using two subrequests to our `/memc` location defined earlier in these two directives.

One can use [HttpLuaModule](http://wiki.nginx.org/HttpLuaModule)'s [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua) or [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) directives to inject custom Lua code to compute the `$backend` and/or `$key` variables in the sample above.

One thing that should be taken care of is that memcached does have restriction on key lengths, i.e., 250 bytes, so for keys that may be very long, one could use the [set_md5](http://wiki.nginx.org/HttpSetMiscModule#set_md5) directive or its friends to pre-hash the key to a fixed-length digest before assigning it to `$memc_key` in the `/memc` location or the like.

Further, one can utilize the [srcache_fetch_skip](http://wiki.nginx.org/HttpSRCacheModule#srcache_fetch_skip) and [srcache_store_skip](http://wiki.nginx.org/HttpSRCacheModule#srcache_store_skip) directives to control what to cache and what not on a per-request basis, and Lua can also be used here in a similar way. So the possibility is really unlimited.

To maximize speed, we often enable TCP (or Unix Domain Socket) connection pool for our memcached upstreams provided by [HttpUpstreamKeepaliveModule](http://wiki.nginx.org/HttpUpstreamKeepaliveModule), for example,


    upstream moon {
        server 10.62.136.54:11211;
        server unix:/tmp/memcached.sock backup;
        keepalive 512;
    }


where we define a connection pool which holds up to 512 keep-alive connections for our `moon` upstream (cluster).

Directives
==========

srcache_fetch
-------------
**syntax:** *srcache_fetch &lt;method&gt; &lt;uri&gt; &lt;args&gt;?*

**default:** *no*

**context:** *http, server, location, location if*

**phase:** *access tail*

This directive registers an access phase handler that will issue an Nginx subrequest to lookup the cache.

When the subrequest returns status code other than `200`, than a cache miss is signaled and the control flow will continue to the later phases including the content phase configured by [HttpProxyModule](http://wiki.nginx.org/HttpProxyModule), [HttpFcgiModule](http://wiki.nginx.org/HttpFcgiModule), and others. If the subrequest returns `200 OK`, then a cache hit is signaled and this module will send the subrequest's response as the current main request's response to the client directly.

This directive will always run at the end of the access phase, such that [HttpAccessModule](http://wiki.nginx.org/HttpAccessModule)'s [allow](http://wiki.nginx.org/HttpAccessModule#allow) and [deny](http://wiki.nginx.org/HttpAccessModule#deny) will always run *before* this.

You can use the [srcache_fetch_skip](http://wiki.nginx.org/HttpSRCacheModule#srcache_fetch_skip) directive to disable cache look-up selectively.

srcache_fetch_skip
------------------
**syntax:** *srcache_fetch_skip &lt;flag&gt;*

**default:** *srcache_fetch_skip 0*

**context:** *http, server, location, location if*

The `<flag>` argument supports nginx variables. When this argument's value is not empty *and* not equal to `0`, then the fetching process will be unconditionally skipped.

For example, to skip caching requests which have a cookie named `foo` with the value `bar`, we can write


    location / {
        set $key ...;
        set_by_lua $skip '
            if ngx.var.cookie_foo == "bar" then
                return 1
            end
            return 0
        ';

        srcache_fetch_skip $skip;
        srcache_store_skip $skip;

        srcache_fetch GET /memc $key;
        srcache_store GET /memc $key;

        # proxy_pass/fastcgi_pass/content_by_lua/...
    }

where [HttpLuaModule](http://wiki.nginx.org/HttpLuaModule) is used to calculate the value of the `$skip` variable at the (earlier) rewrite phase. Similarly, the `$key` variable can be computed by Lua using the [set_by_lua](http://wiki.nginx.org/HttpLuaModule#set_by_lua) or [rewrite_by_lua](http://wiki.nginx.org/HttpLuaModule#rewrite_by_lua) directive too.

srcache_store
-------------
**syntax:** *srcache_store &lt;method&gt; &lt;uri&gt; &lt;args&gt;?*

**default:** *no*

**context:** *http, server, location, location if*

**phase:** *output filter*

This directive registers an output filter handler that will issue an Nginx subrequest to save the response of the current main request into a cache backend. The status code of the subrequest will be ignored.

You can use the [srcache_store_skip](http://wiki.nginx.org/HttpSRCacheModule#srcache_store_skip) and [srcache_store_max_size](http://wiki.nginx.org/HttpSRCacheModule#srcache_store_max_size) directives to disable caching for certain requests in case of a cache miss.

This directive works in an output filter.

srcache_store_max_size
----------------------
**syntax:** *srcache_store_max_size &lt;size&gt;*

**default:** *srcache_store_max_size 0*

**context:** *http, server, location, location if*

When the response body length is exceeding this size, this module will not try to store the response body into the cache using the subrequest template that is specified in [srcache_store](http://wiki.nginx.org/HttpSRCacheModule#srcache_store).

This is particular useful when using cache storage backend that does have a hard upper limit on the input data. For example, for Memcached server, the limit is usually `1 MB`.

When `0` is specified (the default value), there's no limit check at all.

srcache_store_skip
------------------
**syntax:** *srcache_store_skip &lt;flag&gt;*

**default:** *srcache_store_skip 0*

The `<flag>` argument supports Nginx variables. When this argument's value is not empty *and* not equal to `0`, then the storing process will be unconditionally skipped.

Here's an example using Lua to set $nocache to avoid storing URIs that contain the string "/tmp":


    set_by_lua $nocache '
        if string.match(ngx.var.uri, "/tmp") then
            return 1
        end
        return 0';

    srcache_store_skip $nocache;


Known Issues
============
* On certain systems, enabling aio and/or sendfile may stop [srcache_store](http://wiki.nginx.org/HttpSRCacheModule#srcache_store) from working. You can disable them in the locations configured by [srcache_store](http://wiki.nginx.org/HttpSRCacheModule#srcache_store).

Caveats
=======
* For now, ngx_srcache does not cache response headers. So it's necessary to use the [charset](http://wiki.nginx.org/HttpCharsetModule#charset), [default_type](http://wiki.nginx.org/HttpCoreModule#default_type) and [add_header](http://wiki.nginx.org/HttpHeadersModule#add_header) directives to explicitly set the `Content-Type` header and etc. But if a requested from cache URI resource has an explicit extension the nginx will set up an appropriate default_type according to a settings of [types](http://wiki.nginx.org/HttpCoreModule#types) nginx directive regardless of the default_type here. Therefore, it's probably a bad idea to combine this module with backends that return varying response headers. Support for response header caching is a TODO and you're very welcome to submit patches for this :)
* It's recommended to disable your backend server's gzip compression and use nginx's [HttpGzipModule](http://wiki.nginx.org/HttpGzipModule) to do the job. In case of [HttpProxyModule](http://wiki.nginx.org/HttpProxyModule), you can use the following configure setting to disable backend gzip compression:

    proxy_set_header  Accept-Encoding  "";


Installation
============

It's recommended to install this module as well as the Nginx core and many other goodies via the [ngx_openresty bundle](http://openresty.org). It's the easiest way and most safe way to set things up. See OpenResty's [installation instructions](http://openresty.org/#Installation) for details.

Alternatively, you can build Nginx with this module all by yourself:

* Grab the nginx source code from [nginx.org](http://nginx.org), for example, the version 1.0.6 (see [Nginx Compatibility](http://wiki.nginx.org/HttpSRCacheModule#Compatibility)),
* and then download the latest version of the release tarball of this module from srcache-nginx-module [file list](http://github.com/agentzh/srcache-nginx-module/downloads),
* and finally build the Nginx source with this module

        wget 'http://nginx.org/download/nginx-1.0.6.tar.gz'
        tar -xzvf nginx-1.0.6.tar.gz
        cd nginx-1.0.6/
     
        # Here we assume you would install you nginx under /opt/nginx/.
        ./configure --prefix=/opt/nginx \
             --add-module=/path/to/srcache-nginx-module
    
        make -j2
        make install


Compatibility
=============

The following versions of Nginx should work with this module:

* 1.0.x (last tested: 1.0.6)
* 0.9.x (last tested: 0.9.4)
* 0.8.x (last tested: 0.8.54)
* 0.7.x >= 0.7.46 (last tested: 0.7.68)

Earlier versions of Nginx like 0.6.x and 0.5.x, as well as latest nginx 0.8.42+ will *not* work.

If you find that any particular version of Nginx above 0.7.44 does not work with this module, please consider reporting a bug.

Report Bugs
===========
Although a lot of effort has been put into testing and code tuning, there must be some serious bugs lurking somewhere in this module. So whenever you are bitten by any quirks, please don't hesitate to

* create a ticket on the [issue tracking interface](http://github.com/agentzh/srcache-nginx-module/issues) on GitHub.
* or send a bug report or even patches to the [nginx mailing list](http://mailman.nginx.org/mailman/listinfo/nginx).

Source Repository
=================
Available on github at [agentzh/srcache-nginx-module](http://github.com/agentzh/srcache-nginx-module).

ChangeLog
=========

Test Suite
==========
This module comes with a Perl-driven test suite. The [test cases](http://github.com/agentzh/srcache-nginx-module/tree/master/test/t) are [declarative](http://github.com/agentzh/srcache-nginx-module/blob/master/test/t/main-req.t) too. Thanks to the [Test::Nginx](http://search.cpan.org/perldoc?Test::Base) module in the Perl world.

To run it on your side:

    $ PATH=/path/to/your/nginx-with-srcache-module:$PATH prove -r t

You need to terminate any Nginx processes before running the test suite if you have changed the Nginx server binary.

Because a single nginx server (by default, `localhost:1984`) is used across all the test scripts (`.t` files), it's meaningless to run the test suite in parallel by specifying `-jN` when invoking the `prove` utility.

Some parts of the test suite requires modules [HttpRewriteModule](http://wiki.nginx.org/HttpRewriteModule), [HttpEchoModule](http://wiki.nginx.org/HttpEchoModule), [HttpRdsJsonModule](http://wiki.nginx.org/HttpRdsJsonModule), and [HttpDrizzleModule](http://wiki.nginx.org/HttpDrizzleModule) to be enabled as well when building Nginx.

TODO
====
* add support for headers caching.

Getting involved
================
You'll be very welcomed to submit patches to the author or just ask for a commit bit to the source repository on GitHub.

Author
======
Zhang "agentzh" Yichun (章亦春) <agentzh@gmail.com>

Copyright & License
===================
Copyright (c) 2010, 2011 Taobao Inc., Alibaba Group ( <http://www.taobao.com> ).

Copyright (c) 2010, 2011, Zhang "agentzh" Yichun (章亦春) <agentzh@gmail.com>.

This module is licensed under the terms of the BSD license.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

See Also
========
* [HttpMemcModule](http://wiki.nginx.org/HttpMemcModule)
* [HttpLuaModule](http://wiki.nginx.org/HttpLuaModule)
* [HttpSetMiscModule](http://wiki.nginx.org/HttpSetMiscModule)
* The [ngx_openresty bundle](http://openresty.org)

