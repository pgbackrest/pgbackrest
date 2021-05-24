# Force remove all containers:
# docker rm -f $(docker ps -a -q)
# Force remove all images:
# docker rmi -f $(docker images -q)
#
# export PGB=pgbackrest;export PGB_IMAGE=$PGB/test:u20-main;PGB_REPO=~/Documents/Code/$PGB
# docker build -f $PGB_REPO/test/Dockerfile -t $PGB_IMAGE .
# docker run --privileged -itd --name $PGB-test -h $PGB-test -v $PGB_REPO:/home/docker/$PGB $PGB_IMAGE

FROM ubuntu:focal

# Suppress "dpkg-reconfigure: unable to re-open stdin: No file or directory" warning
RUN export DEBCONF_NONINTERACTIVE_SEEN=true DEBIAN_FRONTEND=noninteractive && \

# Install packages
    apt-get update && \
    apt-get install -y \
        sudo vim htop jq rsync sysstat \
        libdbd-pg-perl libxml-checker-perl libyaml-perl \
        devscripts build-essential lintian git cloc txt2man debhelper libssl-dev zlib1g-dev libperl-dev libxml2-dev liblz4-dev \
        liblz4-tool libpq-dev valgrind lcov autoconf-archive zstd libzstd-dev bzip2 libbz2-dev pkg-config && \

# Install Docker
    groupadd -g5000 docker && \
    curl -fsSL https://get.docker.com | sh && \

# Create docker user
    adduser --ingroup=docker -uid=5000 --disabled-password --gecos \"\" docker && \

# Configure sudo
    echo '%docker ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers && \

# Mount tmpfs at /home/docker/test for faster testing
    sudo -u docker mkdir -m 770 /home/docker/test && \
    echo 'tmpfs /home/docker/test tmpfs size=4096M 0 1' >> /etc/fstab

USER docker
WORKDIR /home/docker

ENTRYPOINT sudo rm -f /var/run/docker.pid && sudo mount -a && sudo dockerd
