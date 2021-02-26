FROM ubuntu:18.04

RUN    apt-get update && \
       apt-get upgrade -y && \
       apt-get install -y rsync git devscripts build-essential valgrind lcov autoconf \
       autoconf-archive libssl-dev zlib1g-dev libxml2-dev libpq-dev pkg-config \
       libxml-checker-perl libyaml-perl libdbd-pg-perl liblz4-dev liblz4-tool \
       zstd libzstd-dev bzip2 libbz2-dev

WORKDIR /pgbackrest

ADD . /pgbackrest

RUN cd /pgbackrest/src && \
       ./configure && \
       make && \
       mv /pgbackrest/src/pgbackrest /usr/bin/pgbackrest && \
       chmod 755 /usr/bin/pgbackrest

ENTRYPOINT [ "/usr/bin/pgbackrest" ]

