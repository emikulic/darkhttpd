# Build environment
FROM alpine AS build
RUN apk add --no-cache build-base
WORKDIR /src
COPY . .
RUN make darkhttpd-static \
 && strip darkhttpd-static

# Just the static binary
FROM scratch
WORKDIR /var/www/htdocs
COPY --from=build /src/darkhttpd-static /darkhttpd
EXPOSE 80
ENTRYPOINT ["/darkhttpd"]
CMD ["."]

