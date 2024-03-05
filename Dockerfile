# Build environment
FROM alpine AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY . .

# Hardening GCC opts taken from these sources:
# https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc/
# https://security.stackexchange.com/q/24444/204684
ENV CFLAGS=" \
  -static                                 \
  -O2                                     \
  -flto                                   \
  -D_FORTIFY_SOURCE=2                     \
  -fstack-clash-protection                \
  -fstack-protector-strong                \
  -pipe                                   \
  -Wall                                   \
  -Werror=format-security                 \
  -Werror=implicit-function-declaration   \
  -Wl,-z,defs                             \
  -Wl,-z,now                              \
  -Wl,-z,relro                            \
  -Wl,-z,noexecstack                      \
"
RUN make darkhttpd \
 && strip darkhttpd

# Just the static binary
FROM scratch
WORKDIR /var/www/htdocs
COPY --from=build --chown=0:0 /src/darkhttpd /darkhttpd
COPY --chown=0:0 docker/passwd /etc/passwd
COPY --chown=0:0 docker/group /etc/group
EXPOSE 80
ENTRYPOINT ["/darkhttpd"]
CMD [".", "--chroot", "--uid", "nobody", "--gid", "nobody"]
