# export PGB=pgbackrest;export PGB_IMAGE=$PGB/test:u22-main;PGB_REPO=~/Documents/Code/$PGB
# docker build --squash -f $PGB_REPO/test/Dockerfile -t $PGB_IMAGE .
# docker run --privileged -itd --name $PGB-test -h $PGB-test -v $PGB_REPO:/home/vagrant/$PGB $PGB_IMAGE
FROM ubuntu:jammy

# Install packages
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y \
        sudo vim htop jq rsync sysstat curl \
        libdbd-pg-perl libxml-checker-perl libyaml-perl \
        devscripts build-essential lintian git cloc txt2man debhelper libssl-dev zlib1g-dev libperl-dev libxml2-dev liblz4-dev \
        liblz4-tool libpq-dev lcov autoconf-archive zstd libzstd-dev bzip2 libbz2-dev pkg-config libyaml-dev libc6-dbg wget meson \
        ccache valgrind tzdata

# Install Docker
RUN groupadd -g5000 docker
RUN curl -fsSL https://get.docker.com | sh

# Create vagrant user
RUN adduser --ingroup=docker -uid=5000 --disabled-password --gecos \"\" vagrant

# Mark git path as safe
RUN sudo -u vagrant git config --global --add safe.directory /home/vagrant/pgbackrest

# Configure sudo
RUN echo '%docker ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers

# Mount tmpfs at /home/vagrant/test for faster testing
RUN sudo -u vagrant mkdir -m 770 /home/vagrant/test
RUN echo 'tmpfs /home/vagrant/test tmpfs size=4096M 0 1' >> /etc/fstab

USER vagrant
WORKDIR /home/vagrant

ENTRYPOINT sudo rm -f /var/run/docker.pid && sudo mount -a && sudo dockerd
