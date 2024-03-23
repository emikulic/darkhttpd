# darkhttpd

https://unix4lyfe.org/darkhttpd/

When you need a web server in a hurry.

Features:

* Simple to set up:
  * Single binary, no other files, no installation needed.
  * Standalone, doesn't need `inetd` or `ucspi-tcp`.
  * No messing around with config files - all you have to specify is the `www` root.
* Written in C - efficient and portable.
* Small memory footprint.
* Event loop, single threaded - no fork() or pthreads.
* Generates directory listings.
* Supports HTTP GET and HEAD requests.
* Supports Range / partial content. (try streaming music files or resuming a download)
* Supports If-Modified-Since.
* Supports Keep-Alive connections.
* Supports IPv6.
* Support arbitrary custom response headers.
* Can serve 301 redirects based on Host header.
* Uses sendfile() on FreeBSD, Solaris and Linux.
* Can use acceptfilter on FreeBSD.
* Can use chroot as non-root on FreeBSD 14+.
* At some point worked on FreeBSD, Linux, OpenBSD, Solaris.
* ISC license.
* suckless.org says [darkhttpd sucks less](http://suckless.org/rocks/).
* Small Docker image (<100KB)

Security:

* Can log accesses, including Referer and User-Agent.
* Can chroot.
* Can drop privileges.
* Impervious to `/../` sniffing.
* Times out idle connections.
* Drops overly long requests.

Limitations:

* Only serves static content - no CGI.

## How to build darkhttpd

Simply run make:

```
make
```

If `cc` is not on your `PATH` as an alias to your C compiler, you may need to specify it. For example,

```
CC=gcc make
```

## How to run darkhttpd

Serve /var/www/htdocs on the default port (80 if running as root, else 8080):

```
./darkhttpd /var/www/htdocs
```

Serve `~/public_html` on port 8081:

```
./darkhttpd ~/public_html --port 8081
```

Only bind to one IP address (useful on multi-homed systems):

```
./darkhttpd ~/public_html --addr 192.168.0.1
```

Serve at most 4 simultaneous connections:

```
./darkhttpd ~/public_html --maxconn 4
```

Log accesses to a file:

```
./darkhttpd ~/public_html --log access.log
```

Chroot for extra security (you need root privs for chroot):

```
./darkhttpd /var/www/htdocs --chroot
```

Use default.htm instead of index.html:

```
./darkhttpd /var/www/htdocs --index default.htm
```

Add mimetypes - in this case, serve .dat files as text/plain:

```
$ cat extramime
text/plain  dat
$ ./darkhttpd /var/www/htdocs --mimetypes extramime
```

Drop privileges:

```
./darkhttpd /var/www/htdocs --uid www --gid www
```

Use acceptfilter (FreeBSD only):

```
kldload accf_http
./darkhttpd /var/www/htdocs --accf
```

Run in the background and create a pidfile:

```
./darkhttpd /var/www/htdocs --pidfile /var/run/httpd.pid --daemon
```

Serve only one file instead of a whole directory:

```
./darkhttpd ~/public_html/index.html --single-file
```

Web forward (301) requests for some hosts:

```
./darkhttpd /var/www/htdocs --forward example.com http://www.example.com \
  --forward secure.example.com https://www.example.com/secure
```

Web forward (301) requests for all hosts:

```
./darkhttpd /var/www/htdocs --forward example.com http://www.example.com \
  --forward-all http://catchall.example.com
```

Arbitrary custom response headers (in this case, allow all cross-origin
requests):

```
./darkhttpd /var/www/htdocs --header 'Access-Control-Allow-Origin: *'
```

Commandline options can be combined:

```
./darkhttpd ~/public_html --port 8080 --addr 127.0.0.1
```

To see a full list of commandline options,
run darkhttpd without any arguments:

```
./darkhttpd
```

## How to run darkhttpd in Docker

First, build the image.
```
docker build -t darkhttpd .
```
Then run using volumes for the served files and port mapping for access.

For example, the following would serve files from the current user's dev/mywebsite directory on http://localhost:8080/
```
docker run -p 8080:80 -v ~/dev/mywebsite:/var/www/htdocs:ro darkhttpd
```

Enjoy.

## How to test darkhttpd

```
make test
```

If that isn't working for you, and you're on FreeBSD, you may need to run something closer to the following:

```
ASAN_OPTIONS=" " PYTHON=python3.11 make test
```
