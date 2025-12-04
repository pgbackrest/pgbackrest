# pgBackRest Docker Image - Azure Blob Storage

Docker image with PostgreSQL 18 and pgBackRest configured for Azure Blob Storage backups. Supports Azure Managed Identity (AMI), SAS tokens, and shared key authentication.

## Build

```bash
docker build -t pgbackrest-test .
```

## Run with Azure

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

## Environment Variables

**Required:**
- `POSTGRES_PASSWORD` - PostgreSQL superuser password

**Azure (Required for Azure backups):**
- `AZURE_ACCOUNT` - Azure storage account name
- `AZURE_CONTAINER` - Blob container name
- `AZURE_KEY` - Authentication key (SAS token or shared key, not needed for Managed Identity)
- `AZURE_KEY_TYPE` - Authentication method: `auto` (Managed Identity), `sas` (SAS Token), or `shared` (Shared Key)
- `AZURE_REPO_PATH` - Path in Azure container (default: `/demo-repo`)

## Authentication Methods

### Managed Identity (`auto`) - Azure VMs/ACI/AKS
No keys required. Most secure option for Azure environments.

**Setup (one-time, requires Azure admin):**
```bash
# Enable Managed Identity on VM
az vm identity assign \
  --name <vm-name> \
  --resource-group <rg-name>

# Get Principal ID and grant Storage Blob Data Contributor role
PRINCIPAL_ID=$(az vm identity show \
  --name <vm-name> \
  --resource-group <rg-name> \
  --query principalId -o tsv)

STORAGE_ACCOUNT_ID=$(az storage account show \
  --name <storage-account> \
  --resource-group <rg-name> \
  --query id -o tsv)

az role assignment create \
  --assignee "$PRINCIPAL_ID" \
  --role "Storage Blob Data Contributor" \
  --scope "$STORAGE_ACCOUNT_ID"
```

**Usage:**
```bash
# On Azure VM
docker run -d \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<account> \
  -e AZURE_CONTAINER=<container> \
  -e AZURE_KEY_TYPE=auto \
  pgbackrest-test

# Azure Container Instance (ACI)
az container create \
  --resource-group <rg-name> \
  --name pgbackrest-demo \
  --image pgbackrest-test \
  --assign-identity \
  --environment-variables \
    POSTGRES_PASSWORD=secret \
    AZURE_ACCOUNT=<account> \
    AZURE_CONTAINER=<container> \
    AZURE_KEY_TYPE=auto \
  --cpu 2 \
  --memory 4 \
  --ports 5432
```

### SAS Token (`sas`) - Recommended for local Docker
```bash
SAS_TOKEN=$(az storage container generate-sas \
  --account-name <account> \
  --name <container> \
  --permissions racwdl \
  --expiry $(date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ) \
  --auth-mode login \
  --as-user \
  -o tsv)

docker run -d \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<account> \
  -e AZURE_CONTAINER=<container> \
  -e AZURE_KEY="$SAS_TOKEN" \
  -e AZURE_KEY_TYPE=sas \
  pgbackrest-test
```

### Shared Key (`shared`)
```bash
STORAGE_KEY=$(az storage account keys list \
  --account-name <account> \
  --resource-group <rg> \
  --query "[0].value" -o tsv)

docker run -d \
  -e POSTGRES_PASSWORD=secret \
  -e AZURE_ACCOUNT=<account> \
  -e AZURE_CONTAINER=<container> \
  -e AZURE_KEY="$STORAGE_KEY" \
  -e AZURE_KEY_TYPE=shared \
  pgbackrest-test
```

## Usage

```bash
# Wait for PostgreSQL initialization (30-60 seconds)
sleep 60

# Create stanza (configures both local repo1 and Azure repo2)
docker exec pgbackrest-demo pgbackrest --stanza=demo stanza-create

# Backup to Azure (repo2)
docker exec pgbackrest-demo pgbackrest --stanza=demo --repo=2 backup

# View backup info
docker exec pgbackrest-demo pgbackrest --stanza=demo info

# Check connection to Azure
docker exec pgbackrest-demo pgbackrest --stanza=demo check
```

## Troubleshooting

**Check container logs:**
```bash
docker logs pgbackrest-demo
```

**Verify Azure configuration:**
```bash
docker exec pgbackrest-demo cat /etc/pgbackrest/pgbackrest.conf | grep repo2
```

**Test Managed Identity (on Azure VM):**
```bash
curl -H "Metadata:true" \
  "http://169.254.169.254/metadata/identity/oauth2/token?api-version=2018-02-01&resource=https://storage.azure.com/"
```

**Verify Managed Identity role assignment:**
```bash
az role assignment list \
  --scope "$STORAGE_ACCOUNT_ID" \
  --assignee "$PRINCIPAL_ID" \
  --output table
```

**Check Azure authentication errors:**
```bash
docker exec pgbackrest-demo cat /var/log/pgbackrest/pgbackrest.log
```

**Verify blob storage access:**
```bash
az storage blob list \
  --account-name <account> \
  --container-name <container> \
  --auth-mode login \
  --output table
```
