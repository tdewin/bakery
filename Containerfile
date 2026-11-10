FROM quay.io/fedora/fedora
WORKDIR /app
RUN dnf -y update && \
    dnf -y install \
        gcc \
        gcc-c++ \
        make \
        binutils && \
    dnf clean all && \
    rm -rf /var/cache/dnf

COPY bakery.c /app
RUN gcc -o /bin/bakery bakery.c
COPY test.md /app
RUN bakery test.md && bakery -y exec 1 test.md && bakery -y exec 2 test.md

