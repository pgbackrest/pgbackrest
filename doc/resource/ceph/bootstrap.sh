#!/bin/bash
#
# Start a throwaway single-node Ceph cluster + RGW S3 endpoint in one container.
#
# Uses the stock multi-arch quay.io/ceph/ceph image (no demo image required) with cephx disabled and a bluestore OSD backed by a
# sparse file, so no block device, loop device, privileged mode, or keyrings are needed. Intended only for documentation and test S3
# hosts where the data is ephemeral.
#
# Configured via environment:
#   RGW_DNS_NAME   - virtual-host base name RGW answers for (default s3.us-east-1.amazonaws.com)
#   S3_ACCESS_KEY  - access key for the created user (default accessKey1)
#   S3_SECRET_KEY  - secret key for the created user (default verySecretKey1)
#
# RGW serves HTTPS on :443 using the certificate/key mounted at /root/s3-server.crt and /root/s3-server.key.
set -e

# Parameters
MON_IP=127.0.0.1
HOST=$(hostname -s)
FSID=$(uuidgen)
: "${RGW_DNS_NAME:=s3.us-east-1.amazonaws.com}"
: "${S3_ACCESS_KEY:=accessKey1}"
: "${S3_SECRET_KEY:=verySecretKey1}"

# Configuration
cat > /etc/ceph/ceph.conf <<EOF
[global]
fsid = $FSID
mon initial members = $HOST
mon host = $MON_IP
public network = 127.0.0.0/8
auth cluster required = none
auth service required = none
auth client required = none
osd pool default size = 1
osd pool default min size = 1
osd crush chooseleaf type = 0
osd objectstore = bluestore
osd memory target = 1073741824
mon allow pool size one = true
mon warn on pool no redundancy = false

[mon.$HOST]
host = $HOST
mon addr = $MON_IP:6789

[client.rgw.$HOST]
host = $HOST
rgw dns name = $RGW_DNS_NAME
rgw frontends = beast ssl_endpoint=0.0.0.0:443 ssl_certificate=/root/s3-server.crt ssl_private_key=/root/s3-server.key
log file = /var/log/ceph/rgw.log
EOF

# Monitor
monmaptool --create --add "$HOST" "$MON_IP" --fsid "$FSID" /tmp/monmap >/dev/null
mkdir -p "/var/lib/ceph/mon/ceph-$HOST"
ceph-mon --mkfs -i "$HOST" --monmap /tmp/monmap >/dev/null 2>&1
ceph-mon -i "$HOST"

# Manager
mkdir -p "/var/lib/ceph/mgr/ceph-$HOST"
ceph-mgr -i "$HOST"

# Wait for the monitor to form quorum
for _ in $(seq 1 30); do ceph -s >/dev/null 2>&1 && break; sleep 1; done

# OSD backed by a sparse file on disk (no block device, loop, or privileged mode needed). The file is sized to most of the
# available disk so large-scale tests are bounded by real disk rather than an arbitrary cap, and is sparse so it only consumes the
# data actually written.
OSD_ID=$(ceph osd create)
OSD_PATH="/var/lib/ceph/osd/ceph-$OSD_ID"
mkdir -p "$OSD_PATH"
truncate -s "$(( $(df -B1 "$OSD_PATH" | awk 'NR==2 {print $4}') * 9 / 10 ))" "$OSD_PATH/block"
ceph-osd -i "$OSD_ID" --mkfs >/dev/null 2>&1
ceph-osd -i "$OSD_ID"

# Wait for the OSD to come up
for _ in $(seq 1 30); do ceph osd stat 2>/dev/null | grep -q '1 up' && break; sleep 1; done

# Create the S3 user
radosgw-admin user create \
    --uid=demo --display-name="demo" --access-key="$S3_ACCESS_KEY" --secret-key="$S3_SECRET_KEY" >/dev/null 2>&1

# Run RGW in the foreground to keep the container alive
exec radosgw -n "client.rgw.$HOST" -f
