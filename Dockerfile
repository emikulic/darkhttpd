# Build environment
FROM alpine AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY . .

# Hardening GCC opts taken from these sources:
# https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc/
# https://security.stackexchange.com/q/24444/204684
# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html

#GCC only
#-fzero-init-padding-bits=all
#x86-64 only
#-fcf-protection=full
ENV CFLAGS=" \
  -static                                 \
  -O2                                     \
  -flto                                   \
  -D_FORTIFY_SOURCE=3                     \
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
  -U_FORTIFY_SOURCE                       \
  -D_GLIBCXX_ASSERTIONS                   \
  -fstrict-flex-arrays=3                  \
  -Wl,-z,nodlopen                         \
  -Wl,--as-needed                         \
  -Wl,--no-copy-dt-needed-entries         \
  -fPIE -pie                              \
  -fno-delete-null-pointer-checks         \
  -fno-strict-overflow                    \
  -fno-strict-aliasing                    \
  -ftrivial-auto-var-init=zero            \
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
