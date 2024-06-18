# pgBackRest Docker Image

Complete guide for running pgBackRest with PostgreSQL in Docker, supporting local backups and Azure Blob Storage integration.

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Deployment Scenarios](#deployment-scenarios)
   - [Scenario 1: Local Backups Only](#scenario-1-local-backups-only)
   - [Scenario 2: Azure Blob Storage from Local System](#scenario-2-azure-blob-storage-from-local-system)
   - [Scenario 3: Azure Managed Identity](#scenario-3-azure-managed-identity)
4. [Building the Image](#building-the-image)
5. [Running the Container](#running-the-container)
6. [Authentication Methods](#authentication-methods)
7. [Usage Examples](#usage-examples)
8. [Testing Backups](#testing-backups)
9. [Troubleshooting](#troubleshooting)

---

## Overview

This Docker image provides:
- **PostgreSQL 18** database server
- **pgBackRest** backup and restore tool
- **Automatic WAL archiving** configuration
- **Optional Azure Blob Storage** integration
- **Three deployment scenarios** for different use cases

### Features

- ✅ Local backups (repo1) - Always available
- ✅ Azure Blob Storage (repo2) - Optional, configurable at runtime
- ✅ Multiple authentication methods (Managed Identity, SAS Token, Shared Key)
- ✅ Automatic configuration via environment variables
- ✅ Works on Mac, Linux, Windows, and Azure

---

## Quick Start

### Prerequisites

- Docker installed and running
- (Optional) Azure Storage Account for cloud backups

### Build the Image

```bash
docker build -t pgbackrest-test .
```

### Run Container (Local Backups Only)

```bash
docker run -d \
  --name pgbackrest-demo \
  -e POSTGRES_PASSWORD=secret \
  -p 5432:5432 \
  pgbackrest-test
```

### Run Container (With Azure Blob Storage)

```bash
# Generate SAS token first (see Authentication Methods section)
docker run -d \
  --name pgbackrest-demo \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY="<sas-token>" \
  -e AZURE_KEY_TYPE=sas \
  -p 5432:5432 \
  pgbackrest-test
```

---

## Deployment Scenarios

### Scenario 1: Local Backups Only

**Best for:** Development, testing, or when cloud storage isn't needed

**Features:**
- Backups stored locally in container volume (`/var/lib/pgbackrest`)
- No external dependencies
- Works on any system (Mac, Linux, Windows)

**Usage:**

```bash
# Build image
docker build -t pgbackrest-test .

# Run container
docker run -d \
  --name pgbackrest-local \
  -e POSTGRES_PASSWORD=secret \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql/data \
  -v pgrepo:/var/lib/pgbackrest \
  pgbackrest-test

# Wait for PostgreSQL to initialize (30-60 seconds)
sleep 60

# Create stanza
docker exec pgbackrest-local pgbackrest --stanza=demo stanza-create

# Take backup
docker exec pgbackrest-local pgbackrest --stanza=demo --repo=1 backup

# View backup info
docker exec pgbackrest-local pgbackrest --stanza=demo info
```

**Configuration:**
- Only `repo1` (local) is configured
- No Azure environment variables needed
- Backups persist in Docker volume `pgrepo`

---

### Scenario 2: Azure Blob Storage from Local System

**Best for:** Running Docker on your local machine (Mac/PC) and storing backups in Azure

**Features:**
- Backups stored in both local (repo1) and Azure (repo2)
- Works from any local system
- Uses SAS Token or Shared Key authentication

**Prerequisites:**
- Azure Storage Account
- Azure CLI installed and logged in (for generating SAS tokens)
- Or storage account key (for Shared Key authentication)

**Usage with SAS Token (Recommended):**

```bash
# 1. Login to Azure
az login

# 2. Generate SAS token (valid for 7 days with --as-user)
SAS_TOKEN=$(az storage container generate-sas \
  --account-name <your-storage-account> \
  --name <your-container> \
  --permissions racwdl \
  --expiry $(date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ) \
  --auth-mode login \
  --as-user \
  -o tsv)

# 3. Build image
docker build -t pgbackrest-test .

# 4. Run container with Azure
docker run -d \
  --name pgbackrest-azure \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY="$SAS_TOKEN" \
  -e AZURE_KEY_TYPE=sas \
  -e AZURE_REPO_PATH=/demo-repo \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql/data \
  -v pgrepo:/var/lib/pgbackrest \
  pgbackrest-test

# 5. Wait for initialization
sleep 60

# 6. Create stanza (creates on both repo1 and repo2)
docker exec pgbackrest-azure pgbackrest --stanza=demo stanza-create

# 7. Backup to Azure (repo2)
docker exec pgbackrest-azure pgbackrest --stanza=demo --repo=2 backup

# 8. View backup info
docker exec pgbackrest-azure pgbackrest --stanza=demo info
```

**Usage with Shared Key:**

```bash
# 1. Get storage account key
STORAGE_KEY=$(az storage account keys list \
  --account-name <your-storage-account> \
  --resource-group <your-resource-group> \
  --query "[0].value" -o tsv)

# 2. Run container
docker run -d \
  --name pgbackrest-azure \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY="$STORAGE_KEY" \
  -e AZURE_KEY_TYPE=shared \
  -p 5432:5432 \
  pgbackrest-test
```

---

### Scenario 3: Azure Managed Identity

**Best for:** Running on Azure VMs, Azure Container Instances, or Azure Kubernetes Service

**Features:**
- No keys or tokens needed
- Most secure option for Azure environments
- Automatic authentication via Azure Managed Identity
- Recommended for production

**Prerequisites:**
- Azure resource (VM/ACI/AKS) with Managed Identity enabled
- Managed Identity has "Storage Blob Data Contributor" role
- Requires Azure Administrator permissions to set up (one-time)

**Setup (One-time, requires Admin):**

```bash
# On Azure VM or from Azure CLI with admin permissions

VM_NAME="<your-vm-name>"
RG_NAME="<your-resource-group>"
STORAGE_ACCOUNT="<your-storage-account>"

# 1. Enable Managed Identity on VM
az vm identity assign \
  --name "$VM_NAME" \
  --resource-group "$RG_NAME"

# 2. Get Principal ID
PRINCIPAL_ID=$(az vm identity show \
  --name "$VM_NAME" \
  --resource-group "$RG_NAME" \
  --query principalId -o tsv)

# 3. Grant Storage Blob Data Contributor role
STORAGE_ACCOUNT_ID=$(az storage account show \
  --name "$STORAGE_ACCOUNT" \
  --resource-group "$RG_NAME" \
  --query id -o tsv)

az role assignment create \
  --assignee "$PRINCIPAL_ID" \
  --role "Storage Blob Data Contributor" \
  --scope "$STORAGE_ACCOUNT_ID"
```

**Usage:**

```bash
# Build image
docker build -t pgbackrest-test .

# Run with Managed Identity (no keys needed!)
docker run -d \
  --name pgbackrest-ami \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY_TYPE=auto \
  -e AZURE_REPO_PATH=/demo-repo \
  -p 5432:5432 \
  pgbackrest-test

# Create stanza and backup
docker exec pgbackrest-ami pgbackrest --stanza=demo stanza-create
docker exec pgbackrest-ami pgbackrest --stanza=demo --repo=2 backup
```

**For Azure Container Instance (ACI):**

```bash
az container create \
  --resource-group "$RG_NAME" \
  --name pgbackrest-demo \
  --image pgbackrest-test \
  --assign-identity \
  --environment-variables \
    POSTGRES_PASSWORD=secret \
    AZURE_ACCOUNT=<your-storage-account> \
    AZURE_CONTAINER=<your-container> \
    AZURE_KEY_TYPE=auto \
    AZURE_REPO_PATH=/demo-repo \
  --cpu 2 \
  --memory 4 \
  --ports 5432
```

---

## Building the Image

### Basic Build

```bash
docker build -t pgbackrest-test .
```

### Build with Build Arguments (Optional)

```bash
# Build with Azure credentials at build time (not recommended - use runtime env vars instead)
docker build \
  --build-arg AZURE_ACCOUNT=<your-storage-account> \
  --build-arg AZURE_CONTAINER=<your-container> \
  --build-arg AZURE_KEY_TYPE=auto \
  -t pgbackrest-test .
```

**Note:** It's recommended to use environment variables at runtime rather than build arguments for credentials.

---

## Running the Container

### Environment Variables

#### Required

- `POSTGRES_PASSWORD` - PostgreSQL superuser password

#### Optional (for Azure Blob Storage)

- `AZURE_ACCOUNT` - Azure storage account name
- `AZURE_CONTAINER` - Blob container name
- `AZURE_KEY` - Authentication key (SAS token or shared key)
- `AZURE_KEY_TYPE` - Authentication type: `auto` (Managed Identity), `sas` (SAS Token), or `shared` (Shared Key)
- `AZURE_REPO_PATH` - Path in Azure container (defaults to `/demo-repo`)

### Port Mapping

- Container port: `5432` (PostgreSQL)
- Map to host: `-p 5432:5432` or `-p 5433:5432` (if 5432 is in use)

### Volume Mounts

```bash
# Recommended: Use named volumes for persistence
-v pgdata:/var/lib/postgresql/data      # PostgreSQL data
-v pgrepo:/var/lib/pgbackrest           # Local backup repository
```

### Complete Run Command

```bash
docker run -d \
  --name pgbackrest-demo \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY="<sas-token>" \
  -e AZURE_KEY_TYPE=sas \
  -e AZURE_REPO_PATH=/demo-repo \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql/data \
  -v pgrepo:/var/lib/pgbackrest \
  pgbackrest-test
```

---

## Authentication Methods

### Method 1: Managed Identity (`auto`) ⭐ Recommended for Azure

**When to use:** Running on Azure VMs, ACI, or AKS

**Advantages:**
- ✅ No keys to manage
- ✅ Most secure
- ✅ Automatic authentication
- ✅ No expiration

**Setup:** Requires one-time admin setup (see Scenario 3)

**Usage:**
```bash
docker run -e AZURE_ACCOUNT=<account> \
           -e AZURE_CONTAINER=<container> \
           -e AZURE_KEY_TYPE=auto \
           pgbackrest-test
```

### Method 2: SAS Token (`sas`) ⭐ Recommended for Local

**When to use:** Running Docker on local machine (Mac/PC)

**Advantages:**
- ✅ Time-limited access
- ✅ Works from anywhere
- ✅ No storage account key needed
- ✅ Can be scoped to specific container

**Generate Token:**

**User delegation (max 7 days):**
```bash
SAS_TOKEN=$(az storage container generate-sas \
  --account-name <your-storage-account> \
  --name <your-container> \
  --permissions racwdl \
  --expiry $(date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ) \
  --auth-mode login \
  --as-user \
  -o tsv)
```

**Account SAS (up to 1 year):**
```bash
SAS_TOKEN=$(az storage container generate-sas \
  --account-name <your-storage-account> \
  --name <your-container> \
  --permissions racwdl \
  --expiry $(date -u -d '+1 year' +%Y-%m-%dT%H:%M:%SZ) \
  --auth-mode login \
  -o tsv)
```

**Usage:**
```bash
docker run -e AZURE_ACCOUNT=<account> \
           -e AZURE_CONTAINER=<container> \
           -e AZURE_KEY="$SAS_TOKEN" \
           -e AZURE_KEY_TYPE=sas \
           pgbackrest-test
```

### Method 3: Shared Key (`shared`)

**When to use:** When you have storage account key access

**Advantages:**
- ✅ Simple setup
- ✅ No expiration
- ✅ Works from anywhere

**Get Key:**
```bash
STORAGE_KEY=$(az storage account keys list \
  --account-name <your-storage-account> \
  --resource-group <your-resource-group> \
  --query "[0].value" -o tsv)
```

**Usage:**
```bash
docker run -e AZURE_ACCOUNT=<account> \
           -e AZURE_CONTAINER=<container> \
           -e AZURE_KEY="$STORAGE_KEY" \
           -e AZURE_KEY_TYPE=shared \
           pgbackrest-test
```

---

## Usage Examples

### Example 1: Complete Local Setup

```bash
# 1. Build
docker build -t pgbackrest-test .

# 2. Run
docker run -d \
  --name pgbackrest-demo \
  -e POSTGRES_PASSWORD=secret \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql/data \
  -v pgrepo:/var/lib/pgbackrest \
  pgbackrest-test

# 3. Wait for initialization
sleep 60

# 4. Create stanza
docker exec pgbackrest-demo pgbackrest --stanza=demo stanza-create

# 5. Take backup
docker exec pgbackrest-demo pgbackrest --stanza=demo --repo=1 backup

# 6. View info
docker exec pgbackrest-demo pgbackrest --stanza=demo info

# 7. Access PostgreSQL
psql -h localhost -p 5432 -U postgres
# Password: secret
```

### Example 2: Complete Azure Setup (SAS Token)

```bash
# 1. Login to Azure
az login

# 2. Generate SAS token
SAS_TOKEN=$(az storage container generate-sas \
  --account-name <your-storage-account> \
  --name <your-container> \
  --permissions racwdl \
  --expiry $(date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ) \
  --auth-mode login \
  --as-user \
  -o tsv)

# 3. Build
docker build -t pgbackrest-test .

# 4. Run with Azure
docker run -d \
  --name pgbackrest-azure \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY="$SAS_TOKEN" \
  -e AZURE_KEY_TYPE=sas \
  -p 5432:5432 \
  -v pgdata:/var/lib/postgresql/data \
  -v pgrepo:/var/lib/pgbackrest \
  pgbackrest-test

# 5. Wait for initialization
sleep 60

# 6. Create stanza (both repos)
docker exec pgbackrest-azure pgbackrest --stanza=demo stanza-create

# 7. Backup to Azure
docker exec pgbackrest-azure pgbackrest --stanza=demo --repo=2 backup

# 8. Backup to local
docker exec pgbackrest-azure pgbackrest --stanza=demo --repo=1 backup

# 9. View info
docker exec pgbackrest-azure pgbackrest --stanza=demo info

# 10. Verify in Azure
az storage blob list \
  --account-name <your-storage-account> \
  --container-name <your-container> \
  --auth-mode login \
  --output table
```

### Example 3: Using the Test Script

```bash
# Set environment variables
export AZURE_ACCOUNT="<your-storage-account>"
export AZURE_CONTAINER="<your-container>"
export AZURE_SAS_TOKEN="<your-sas-token>"

# Run automated test (creates, backs up, restores)
./azure-pgbackrest.sh test
```

### Example 4: Loading Sample Data (Northwind Database)

The Northwind database is a sample database commonly used for testing. Here's how to download and load it:

```bash
# 1. Download Northwind SQL file
curl -o northwind.sql https://raw.githubusercontent.com/pthom/northwind_psql/master/northwind.sql

# Alternative: Use a different source
# wget https://github.com/pthom/northwind_psql/raw/master/northwind.sql

# 2. Copy SQL file into running container
docker cp northwind.sql <container-name>:/tmp/northwind.sql

# 3. Create Northwind database
docker exec <container-name> psql -U postgres -c "CREATE DATABASE northwind;"

# 4. Load Northwind data
docker exec <container-name> psql -U postgres -d northwind -f /tmp/northwind.sql

# 5. Verify data loaded
docker exec <container-name> psql -U postgres -d northwind -c "SELECT COUNT(*) FROM customers;"

# 6. Take backup of Northwind database
# Note: pgBackRest backs up all databases in the cluster
docker exec <container-name> pgbackrest --stanza=demo --repo=1 backup
```

**Alternative: Load during container initialization**

```bash
# 1. Download Northwind SQL file
curl -o northwind.sql https://raw.githubusercontent.com/pthom/northwind_psql/master/northwind.sql

# 2. Mount SQL file and use init script
docker run -d \
  --name pgbackrest-demo \
  -e POSTGRES_PASSWORD=secret \
  -v $(pwd)/northwind.sql:/docker-entrypoint-initdb.d/northwind.sql \
  -p 5432:5432 \
  pgbackrest-test

# The SQL file will be executed automatically during database initialization
```

**Note:** The Northwind database is not included in the Docker image. You need to download it separately if you want to use it for testing.

---

## Testing Backups

### Verify Configuration

```bash
# Check pgBackRest config
docker exec <container> cat /etc/pgbackrest/pgbackrest.conf

# Should show repo1 (local) and optionally repo2 (Azure)
```

### Create Stanza

```bash
docker exec <container> pgbackrest --stanza=demo stanza-create
```

### Take Backup

```bash
# Backup to local (repo1)
docker exec <container> pgbackrest --stanza=demo --repo=1 backup

# Backup to Azure (repo2)
docker exec <container> pgbackrest --stanza=demo --repo=2 backup
```

### View Backup Information

```bash
docker exec <container> pgbackrest --stanza=demo info
```

### Test Connection

```bash
# Test both repositories
docker exec <container> pgbackrest --stanza=demo check
```

### Restore from Backup

```bash
# Stop container
docker stop <container>

# Restore (using a temporary container)
docker run --rm \
  --entrypoint bash \
  -e AZURE_ACCOUNT=<your-storage-account> \
  -e AZURE_CONTAINER=<your-container> \
  -e AZURE_KEY="<sas-token>" \
  -e AZURE_KEY_TYPE=sas \
  -v pgdata:/var/lib/postgresql/data \
  -v pgrepo:/var/lib/pgbackrest \
  pgbackrest-test \
  -lc "/usr/local/bin/configure-azure.sh && \
       rm -rf /var/lib/postgresql/data/* && \
       pgbackrest --stanza=demo restore --set=<backup-label> --type=immediate"

# Start container
docker start <container>
```

---

## Troubleshooting

### Container Won't Start

**Check logs:**
```bash
docker logs <container>
```

**Common issues:**
- PostgreSQL initialization takes 30-60 seconds - wait longer
- Port conflict - use `-p 5433:5432` instead
- Volume permissions - ensure Docker has access

### Azure Not Configured

**Check if Azure config is present:**
```bash
docker exec <container> cat /etc/pgbackrest/pgbackrest.conf | grep repo2
```

**If missing:**
- Verify environment variables are set correctly
- Check container logs for configuration errors
- Ensure `AZURE_ACCOUNT` and `AZURE_CONTAINER` are provided

### Azure Authentication Fails

**For Managed Identity:**
```bash
# Verify Managed Identity is enabled (on Azure VM)
curl -H "Metadata:true" \
  "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https://storage.azure.com/"

# Check role assignment
az role assignment list \
  --scope "$STORAGE_ACCOUNT_ID" \
  --assignee "$PRINCIPAL_ID" \
  --output table
```

**For SAS Token:**
- Verify token hasn't expired
- Check token permissions include `racwdl`
- Regenerate token if needed

**For Shared Key:**
- Verify key is correct
- Ensure key is base64-encoded (Azure keys are already encoded)

### PostgreSQL in Recovery Mode

**Check status:**
```bash
docker exec <container> psql -U postgres -c "SELECT pg_is_in_recovery();"
```

**If in recovery:**
- Container may have been restored from backup
- Promote to primary: `docker exec <container> psql -U postgres -c "SELECT pg_wal_replay_resume();"`
- Or restart with clean volume

### Backup Fails

**Check pgBackRest logs:**
```bash
docker exec <container> cat /var/log/pgbackrest/pgbackrest.log
```

**Common errors:**
- `[028]: backup and archive info files exist but do not match` - Clean old backups
- `[032]: key '2' is not valid` - Use correct repo syntax
- Permission denied - Check Azure credentials

### Clean Up and Start Fresh

```bash
# Stop and remove container
docker stop <container>
docker rm <container>

# Remove volumes (⚠️ deletes all data)
docker volume rm pgdata pgrepo

# Start fresh
docker run -d --name pgbackrest-demo ...
```

### Request Admin Help for Managed Identity

If you need Managed Identity but don't have admin permissions, send this to your Azure Administrator:

**Subject: Request to Enable Azure Managed Identity for pgBackRest**

Hi,

I need Azure Managed Identity enabled on VM `<your-vm-name>` (resource group: `<your-resource-group>`) to allow pgBackRest to authenticate to Azure Blob Storage without storing credentials.

**Error encountered:**
```
(AuthorizationFailed) The client does not have authorization to perform action 'Microsoft.Compute/virtualMachines/read'
```

**Commands to run (requires Owner/User Access Administrator permissions):**

```bash
VM_NAME="<your-vm-name>"
RG_NAME="<your-resource-group>"
STORAGE_ACCOUNT="<your-storage-account>"

# Enable Managed Identity
az vm identity assign --name "$VM_NAME" --resource-group "$RG_NAME"

# Get Principal ID and grant role
PRINCIPAL_ID=$(az vm identity show --name "$VM_NAME" --resource-group "$RG_NAME" --query principalId -o tsv)
STORAGE_ACCOUNT_ID=$(az storage account show --name "$STORAGE_ACCOUNT" --resource-group "$RG_NAME" --query id -o tsv)
az role assignment create --assignee "$PRINCIPAL_ID" --role "Storage Blob Data Contributor" --scope "$STORAGE_ACCOUNT_ID"
```

This allows pgBackRest to use `AZURE_KEY_TYPE=auto` instead of SAS tokens. No downtime expected.

Thanks!

---

## Configuration File

The pgBackRest configuration is automatically generated at `/etc/pgbackrest/pgbackrest.conf`:

**Local only (Scenario 1):**
```ini
[global]
repo1-path=/var/lib/pgbackrest
log-path=/var/log/pgbackrest
log-level-console=info
log-level-file=info
repo1-retention-full=2

[demo]
pg1-path=/var/lib/postgresql/data
```

**With Azure (Scenario 2 or 3):**
```ini
[global]
repo1-path=/var/lib/pgbackrest
log-path=/var/log/pgbackrest
log-level-console=info
log-level-file=info
repo1-retention-full=2

repo2-type=azure
repo2-azure-account=<your-storage-account>
repo2-azure-container=<your-container>
repo2-azure-key-type=auto    # or "shared" or "sas"
repo2-azure-key=<key>        # Only for shared/sas, not for auto
repo2-path=/demo-repo
repo2-retention-full=4

[demo]
pg1-path=/var/lib/postgresql/data
```

---

## Comparison Table

| Scenario | Works On | Security | Key Management | Best For | Setup Complexity |
|----------|----------|----------|----------------|----------|------------------|
| **Local Only** | Anywhere | ⭐⭐⭐ | None | Development | Easy |
| **Azure (SAS)** | Anywhere | ⭐⭐⭐⭐ | Expires (7 days with --as-user, 1 year without) | Local dev, testing | Easy |
| **Azure (Shared Key)** | Anywhere | ⭐⭐⭐ | Manual rotation | Local dev | Easy |
| **Azure (Managed Identity)** | Azure only | ⭐⭐⭐⭐⭐ | None needed | Production on Azure | Requires Admin (one-time) |

---

## Next Steps

- See `azure-pgbackrest.sh` for automated testing script
- See `Dockerfile` for build configuration details
- See `AZURE_BLOB_STORAGE.md` for detailed Azure setup (if needed)

---

## Summary

### Quick Reference

**Local Backups:**
```bash
docker run -e POSTGRES_PASSWORD=secret pgbackrest-test
```

**Azure with SAS Token:**
```bash
docker run -e POSTGRES_PASSWORD=secret \
           -e AZURE_ACCOUNT=<account> \
           -e AZURE_CONTAINER=<container> \
           -e AZURE_KEY="<sas-token>" \
           -e AZURE_KEY_TYPE=sas \
           pgbackrest-test
```

**Azure with Managed Identity:**
```bash
docker run -e POSTGRES_PASSWORD=secret \
           -e AZURE_ACCOUNT=<account> \
           -e AZURE_CONTAINER=<container> \
           -e AZURE_KEY_TYPE=auto \
           pgbackrest-test
```

