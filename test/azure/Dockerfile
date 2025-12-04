# ============================================================================
# pgBackRest Docker Image
# ============================================================================
# Supports three deployment scenarios:
#   1. Local backups only (Mac/Linux/Windows)
#   2. Azure Blob Storage from local system (SAS Token or Shared Key)
#   3. Azure Managed Identity (Azure VMs/Container Instances/AKS)
#
# See Azure configuration section below for details.
# ============================================================================

# Postgres base image (Debian-based, multi-arch, works on Mac)
FROM postgres:18

# Build args – official pgBackRest repo + main branch
ARG PGBR_REPO="https://github.com/pgEdge/pgbackrest.git"
ARG PGBR_BRANCH="azure-managed-identities"

# ============================================================================
# Azure Blob Storage Configuration (Optional)
# ============================================================================
# Azure configuration is done at runtime via environment variables for security.
# See DOCKER_README.md for usage examples and authentication methods.
#
# All Azure credentials (keys, tokens) should be provided at runtime, not build time.
# No build-time ARGs for sensitive data to avoid security warnings.
# ============================================================================

USER root

# Install build deps for pgBackRest
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      git \
      ca-certificates \
      meson \
      ninja-build \
      gcc \
      g++ \
      make \
      pkg-config \
      libpq-dev \
      libssl-dev \
      libxml2-dev \
      liblz4-dev \
      libzstd-dev \
      libbz2-dev \
      zlib1g-dev \
      libyaml-dev \
      libssh2-1-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Clone pgBackRest main and build
RUN git clone --branch "${PGBR_BRANCH}" --single-branch "${PGBR_REPO}" pgbackrest && \
    meson setup /build/pgbackrest-build /build/pgbackrest --buildtype=release && \
    ninja -C /build/pgbackrest-build && \
    ninja -C /build/pgbackrest-build install

# pgBackRest config
RUN mkdir -p /etc/pgbackrest /var/lib/pgbackrest /var/log/pgbackrest && \
    chown -R postgres:postgres /var/lib/pgbackrest /var/log/pgbackrest /etc/pgbackrest

# Create base config (without Azure)
# Azure configuration is done at runtime via environment variables
RUN printf '%s\n' \
      '[global]' \
      'repo1-path=/var/lib/pgbackrest' \
      'log-path=/var/log/pgbackrest' \
      'lock-path=/var/lib/pgbackrest' \
      'log-level-console=info' \
      'log-level-file=info' \
      'repo1-retention-full=2' \
      '' \
      '[demo]' \
      'pg1-path=/var/lib/postgresql/data' \
      > /etc/pgbackrest/pgbackrest.conf && \
    chown postgres:postgres /etc/pgbackrest/pgbackrest.conf && \
    chmod 660 /etc/pgbackrest/pgbackrest.conf

# Create script to configure Azure at runtime via environment variables
RUN cat > /usr/local/bin/configure-azure.sh <<'SCRIPT_EOF'
#!/bin/bash
set -e
if [ -n "$AZURE_ACCOUNT" ] && [ -n "$AZURE_CONTAINER" ]; then
  # Check if key is required (not needed for auto/Managed Identity)
  AZURE_KEY_TYPE=${AZURE_KEY_TYPE:-auto}
  if [ "$AZURE_KEY_TYPE" != "auto" ] && [ -z "$AZURE_KEY" ]; then
    echo "Error: AZURE_KEY is required for key type: ${AZURE_KEY_TYPE}"
    exit 1
  fi
  
  # Remove existing Azure repo2 config if present (from repo2-type to next blank line or end)
  awk '
    /^\[/ { in_azure=0 }
    /^repo2-type=azure/ { in_azure=1; next }
    in_azure && /^repo2-/ { next }
    in_azure && /^$/ { in_azure=0 }
    !in_azure { print }
  ' /etc/pgbackrest/pgbackrest.conf > /tmp/pgbackrest.conf.tmp
  mv /tmp/pgbackrest.conf.tmp /etc/pgbackrest/pgbackrest.conf || true
  
  # Add Azure config
  AZURE_REPO_PATH=${AZURE_REPO_PATH:-/demo-repo}
  printf '\n%s\n' \
    'repo2-type=azure' \
    "repo2-azure-account=${AZURE_ACCOUNT}" \
    "repo2-azure-container=${AZURE_CONTAINER}" \
    "repo2-azure-key-type=${AZURE_KEY_TYPE}" \
    "repo2-path=${AZURE_REPO_PATH}" \
    'repo2-retention-full=4' \
    >> /etc/pgbackrest/pgbackrest.conf
  
  # Add key only if not using auto (Managed Identity)
  if [ "$AZURE_KEY_TYPE" != "auto" ] && [ -n "$AZURE_KEY" ]; then
    echo "repo2-azure-key=${AZURE_KEY}" >> /etc/pgbackrest/pgbackrest.conf
  fi
  
  echo "Azure storage configured successfully"
  echo "Account: ${AZURE_ACCOUNT}"
  echo "Container: ${AZURE_CONTAINER}"
  echo "Key type: ${AZURE_KEY_TYPE}"
  if [ "$AZURE_KEY_TYPE" = "auto" ]; then
    echo "Using Azure Managed Identity authentication"
  fi
else
  echo "Azure credentials not provided. Skipping Azure configuration."
fi
SCRIPT_EOF
RUN chmod +x /usr/local/bin/configure-azure.sh

# Enable archive_mode + archive_command on first initdb
# This script configures PostgreSQL for WAL archiving and optionally Azure storage
RUN mkdir -p /docker-entrypoint-initdb.d && \
    cat >/docker-entrypoint-initdb.d/pgbackrest-archive.sh <<'EOF'
#!/bin/bash
set -e

# Configure Azure storage if environment variables are provided
# Supports all three scenarios:
#   1. No Azure vars = local backups only (repo1)
#   2. Azure vars with shared/sas = Azure from local system (repo1 + repo2)
#   3. Azure vars with auto = Azure Managed Identity (repo1 + repo2)
if [ -n "$AZURE_ACCOUNT" ] && [ -n "$AZURE_CONTAINER" ]; then
  /usr/local/bin/configure-azure.sh
  echo "Azure Blob Storage (repo2) configured"
else
  echo "Azure not configured - using local backups only (repo1)"
fi

# Update pg1-path in pgBackRest config to use actual PGDATA path (PostgreSQL 18 compatibility)
# PGDATA is set by the postgres image - use it directly
if [ -n "$PGDATA" ]; then
  PG_DATA_DIR="$PGDATA"
  echo "Using PGDATA: $PG_DATA_DIR"
else
  # Fallback: try to find the data directory
  if [ -d "/var/lib/postgresql/18/main" ]; then
    PG_DATA_DIR="/var/lib/postgresql/18/main"
  elif [ -d "/var/lib/postgresql/data" ]; then
    PG_DATA_DIR="/var/lib/postgresql/data"
  else
    PG_DATA_DIR="/var/lib/postgresql/data"
  fi
  echo "Using detected path: $PG_DATA_DIR"
fi

# Update pgBackRest config with the correct path
# Use a temp file approach to avoid permission issues with sed -i
sed '/^pg1-path=/d' /etc/pgbackrest/pgbackrest.conf > /tmp/pgbackrest.conf.tmp
if grep -q "^\[demo\]" /tmp/pgbackrest.conf.tmp; then
  sed '/^\[demo\]/a pg1-path='"$PG_DATA_DIR" /tmp/pgbackrest.conf.tmp > /tmp/pgbackrest.conf.tmp2
  mv /tmp/pgbackrest.conf.tmp2 /tmp/pgbackrest.conf.tmp
else
  echo "" >> /tmp/pgbackrest.conf.tmp
  echo "[demo]" >> /tmp/pgbackrest.conf.tmp
  echo "pg1-path=$PG_DATA_DIR" >> /tmp/pgbackrest.conf.tmp
fi
mv /tmp/pgbackrest.conf.tmp /etc/pgbackrest/pgbackrest.conf
echo "Updated pgBackRest config: pg1-path=$PG_DATA_DIR"

# Configure PostgreSQL for archiving (required for all scenarios)
echo "archive_mode = on" >> "$PG_DATA_DIR/postgresql.conf"
echo "archive_command = 'pgbackrest --stanza=demo archive-push %p'" >> "$PG_DATA_DIR/postgresql.conf"
echo "archive_timeout = 60" >> "$PG_DATA_DIR/postgresql.conf"
echo "wal_level = replica" >> "$PG_DATA_DIR/postgresql.conf"
echo "max_wal_senders = 3" >> "$PG_DATA_DIR/postgresql.conf"
echo "max_replication_slots = 3" >> "$PG_DATA_DIR/postgresql.conf"
EOF
RUN chmod +x /docker-entrypoint-initdb.d/pgbackrest-archive.sh

USER postgres
EXPOSE 5432
# ENTRYPOINT and CMD come from postgres:18
