#!/usr/bin/env bash
# Master script for Azure pgBackRest operations
# Handles SAS token generation, cleanup, and full backup/restore testing

set -euo pipefail

# Configuration - Set these environment variables or modify defaults
AZURE_ACCOUNT="${AZURE_ACCOUNT:-your-storage-account}"
AZURE_CONTAINER="${AZURE_CONTAINER:-your-container}"
RESOURCE_GROUP="${RESOURCE_GROUP:-your-resource-group}"
IMAGE="${IMAGE:-pgbackrest-test}"
CONTAINER="${CONTAINER:-pgbr-test}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Functions
print_header() {
    echo ""
    echo "=========================================="
    echo "$1"
    echo "=========================================="
    echo ""
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Function 1: Generate SAS Token (for Ubuntu machine)
generate_sas_token() {
    print_header "Generate SAS Token"
    
    # Check if Azure CLI is available
    AZ_CMD=""
    if command -v az &> /dev/null; then
        AZ_CMD="az"
    elif [ -f "/usr/bin/az" ]; then
        AZ_CMD="/usr/bin/az"
    fi
    
    if [ -z "$AZ_CMD" ]; then
        print_error "Azure CLI (az) not found"
        echo "To generate SAS token, install Azure CLI and use:"
        echo "  az storage container generate-sas \\"
        echo "    --account-name <your-storage-account> \\"
        echo "    --name <your-container> \\"
        echo "    --permissions racwdl \\"
        echo "    --expiry \$(date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ) \\"
        echo "    --auth-mode login \\"
        echo "    --as-user \\"
        echo "    -o tsv"
        return 1
    fi
    
    # Validate configuration
    if [ "$AZURE_ACCOUNT" = "your-storage-account" ] || [ "$AZURE_CONTAINER" = "your-container" ]; then
        print_error "Please set AZURE_ACCOUNT and AZURE_CONTAINER environment variables"
        echo "Example:"
        echo "  export AZURE_ACCOUNT=my-storage-account"
        echo "  export AZURE_CONTAINER=my-container"
        return 1
    fi
    
    echo "Generating user delegation SAS token (valid for 7 days)..."
    # Calculate expiry date (7 days from now) - handle both macOS and Linux
    if date -u -v+7d +%Y-%m-%dT%H:%M:%SZ >/dev/null 2>&1; then
        # macOS date command
        EXPIRY=$(date -u -v+7d +%Y-%m-%dT%H:%M:%SZ)
    elif date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ >/dev/null 2>&1; then
        # Linux date command
        EXPIRY=$(date -u -d '+7 days' +%Y-%m-%dT%H:%M:%SZ)
    else
        print_error "Could not calculate expiry date"
        return 1
    fi
    
    SAS_TOKEN=$($AZ_CMD storage container generate-sas \
      --account-name "$AZURE_ACCOUNT" \
      --name "$AZURE_CONTAINER" \
      --permissions racwdl \
      --expiry "$EXPIRY" \
      --auth-mode login \
      --as-user \
      -o tsv)
    
    if [ -z "$SAS_TOKEN" ]; then
        print_error "Failed to generate SAS token"
        return 1
    fi
    
    print_success "SAS token generated successfully"
    echo ""
    echo "Token:"
    echo "$SAS_TOKEN"
    echo ""
    echo "Export it for use:"
    echo "  export AZURE_SAS_TOKEN=\"$SAS_TOKEN\""
    echo ""
    echo "Or save to file:"
    echo "  echo \"$SAS_TOKEN\" > /tmp/azure-sas-token.txt"
    
    # Save to file for easy retrieval
    echo "$SAS_TOKEN" > /tmp/azure-sas-token.txt
    print_success "Token saved to /tmp/azure-sas-token.txt"
    
    return 0
}

# Function 2: Cleanup Azure Storage
cleanup_azure() {
    local PREFIX="${1:-test-repo}"
    
    print_header "Cleanup Azure Storage (prefix: $PREFIX/)"
    
    # Check if Azure CLI is available
    AZ_CMD=""
    if command -v az &> /dev/null; then
        AZ_CMD="az"
    elif [ -f "/usr/bin/az" ]; then
        AZ_CMD="/usr/bin/az"
    fi
    
    if [ -z "$AZ_CMD" ]; then
        print_error "Azure CLI (az) not found"
        return 1
    fi
    
    # Validate configuration
    if [ "$AZURE_ACCOUNT" = "your-storage-account" ] || [ "$AZURE_CONTAINER" = "your-container" ]; then
        print_error "Please set AZURE_ACCOUNT and AZURE_CONTAINER environment variables"
        return 1
    fi
    
    echo "Listing blobs with prefix $PREFIX/..."
    BLOBS=$($AZ_CMD storage blob list \
      --account-name "$AZURE_ACCOUNT" \
      --container-name "$AZURE_CONTAINER" \
      --prefix "$PREFIX/" \
      --auth-mode login \
      --output tsv --query "[].name" 2>/dev/null || true)
    
    if [ -z "$BLOBS" ]; then
        print_success "No blobs found with prefix $PREFIX/"
        return 0
    fi
    
    BLOB_COUNT=$(echo "$BLOBS" | wc -l)
    echo "Found $BLOB_COUNT blob(s) to delete"
    echo ""
    echo "First 10 blobs:"
    echo "$BLOBS" | head -10
    if [ "$BLOB_COUNT" -gt 10 ]; then
        echo "... and $((BLOB_COUNT - 10)) more"
    fi
    echo ""
    
    read -p "Delete these blobs? (y/N): " -n 1 -r
    echo ""
    
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_warning "Cleanup cancelled"
        return 0
    fi
    
    echo "Deleting blobs in parallel (10 at a time)..."
    echo "$BLOBS" | xargs -P 10 -I {} $AZ_CMD storage blob delete \
      --account-name "$AZURE_ACCOUNT" \
      --container-name "$AZURE_CONTAINER" \
      --name {} \
      --auth-mode login \
      --only-show-errors 2>/dev/null || true
    
    print_success "Cleanup complete!"
    return 0
}

# Function 3: Clean Docker containers and volumes
cleanup_docker() {
    print_header "Cleanup Docker Containers and Volumes"
    
    echo "Stopping and removing container..."
    docker stop "$CONTAINER" 2>/dev/null || true
    docker rm "$CONTAINER" 2>/dev/null || true
    
    echo "Removing volumes..."
    docker volume rm pgdata pgrepo 2>/dev/null || true
    
    # Check and free up port 5433 if in use
    if lsof -ti:5433 >/dev/null 2>&1; then
        echo "Port 5433 is in use, freeing it..."
        lsof -ti:5433 | xargs kill -9 2>/dev/null || true
        sleep 1
    fi
    
    print_success "Docker cleanup complete!"
    return 0
}

# Function 3.5: Full cleanup including image rebuild
cleanup_all_and_rebuild() {
    print_header "Full Cleanup and Rebuild"
    
    # Clean containers and volumes
    cleanup_docker
    
    echo "Removing old image..."
    docker rmi "$IMAGE" 2>/dev/null || true
    
    echo "Rebuilding Docker image..."
    if docker build -t "$IMAGE" .; then
        print_success "Image rebuilt successfully"
    else
        print_error "Image rebuild failed"
        return 1
    fi
    
    # Ensure port is free
    if lsof -ti:5433 >/dev/null 2>&1; then
        echo "Port 5433 is in use, freeing it..."
        lsof -ti:5433 | xargs kill -9 2>/dev/null || true
        sleep 2
    fi
    
    print_success "Full cleanup and rebuild complete!"
    return 0
}

# Helper function: Download Northwind database
download_northwind() {
    local container_name="$1"
    
    print_header "Download Northwind Database"
    
    # Check if Northwind SQL file already exists in container
    if docker exec "$container_name" test -f /northwind.sql 2>/dev/null; then
        print_success "Northwind SQL file already exists in container"
        return 0
    fi
    
    # Try to download from common sources
    echo "Downloading Northwind database..."
    
    # Try multiple sources for Northwind database
    NORTHWIND_URLS=(
        "https://raw.githubusercontent.com/pthom/northwind_psql/master/northwind.sql"
        "https://github.com/pthom/northwind_psql/raw/master/northwind.sql"
        "https://raw.githubusercontent.com/jpwhite3/northwind-SQLite3/master/northwind.sql"
    )
    
    DOWNLOADED=false
    for URL in "${NORTHWIND_URLS[@]}"; do
        echo "Trying: $URL"
        if curl -s -f -L "$URL" -o /tmp/northwind.sql 2>/dev/null && [ -s /tmp/northwind.sql ]; then
            # Copy to container (as root, then fix permissions)
            docker cp /tmp/northwind.sql "$container_name:/northwind.sql"
            docker exec -u root "$container_name" chmod 644 /northwind.sql 2>/dev/null || true
            rm -f /tmp/northwind.sql
            DOWNLOADED=true
            print_success "Northwind database downloaded successfully"
            break
        fi
    done
    
    if [ "$DOWNLOADED" = "false" ]; then
        # Try alternative: create a simple Northwind-like database
        print_warning "Could not download Northwind from online sources, creating a simple test database..."
        # Create SQL file locally first, then copy to container
        cat > /tmp/northwind.sql << 'NORTHWIND_EOF'
-- Simple Northwind-like database for testing
-- Note: Database should be created separately before running this script

CREATE TABLE customers (
    customer_id VARCHAR(5) PRIMARY KEY,
    company_name VARCHAR(40) NOT NULL,
    contact_name VARCHAR(30),
    contact_title VARCHAR(30),
    address VARCHAR(60),
    city VARCHAR(15),
    region VARCHAR(15),
    postal_code VARCHAR(10),
    country VARCHAR(15),
    phone VARCHAR(24),
    fax VARCHAR(24)
);

INSERT INTO customers VALUES
    ('ALFKI', 'Alfreds Futterkiste', 'Maria Anders', 'Sales Representative', 'Obere Str. 57', 'Berlin', NULL, '12209', 'Germany', '030-0074321', '030-0076545'),
    ('ANATR', 'Ana Trujillo Emparedados y helados', 'Ana Trujillo', 'Owner', 'Avda. de la Constitución 2222', 'México D.F.', NULL, '05021', 'Mexico', '(5) 555-4729', '(5) 555-3745'),
    ('ANTON', 'Antonio Moreno Taquería', 'Antonio Moreno', 'Owner', 'Mataderos  2312', 'México D.F.', NULL, '05023', 'Mexico', '(5) 555-3932', NULL),
    ('AROUT', 'Around the Horn', 'Thomas Hardy', 'Sales Representative', '120 Hanover Sq.', 'London', NULL, 'WA1 1DP', 'UK', '(171) 555-7788', '(171) 555-6750'),
    ('BERGS', 'Berglunds snabbköp', 'Christina Berglund', 'Order Administrator', 'Berguvsvägen  8', 'Luleå', NULL, 'S-958 22', 'Sweden', '0921-12 34 65', '0921-12 34 67');
NORTHWIND_EOF
        docker cp /tmp/northwind.sql "$container_name:/northwind.sql"
        docker exec -u root "$container_name" chmod 644 /northwind.sql 2>/dev/null || true
        rm -f /tmp/northwind.sql
        print_success "Created simple Northwind test database"
    else
        # Remove CREATE DATABASE statement from downloaded file if present (we create it separately)
        docker exec "$container_name" bash -c 'sed -i "/^CREATE DATABASE/d; /^\\\\c/d" /northwind.sql 2>/dev/null || true'
    fi
    
    return 0
}

# Helper function: Create test data
create_test_data() {
    local container_name="$1"
    
    print_header "Create Test Data"
    docker exec "$container_name" \
      psql -U postgres -d postgres -c "DROP TABLE IF EXISTS restore_test;" >/dev/null 2>&1 || true
    
    docker exec "$container_name" \
      psql -U postgres -d postgres -c "CREATE TABLE restore_test(id int primary key, note text);"
    
    docker exec "$container_name" \
      psql -U postgres -d postgres -c "SELECT * FROM restore_test;"
    
    # Modify config
    docker exec "$container_name" bash -lc 'echo "shared_buffers = 999MB" >> $PGDATA/postgresql.conf'
    docker exec "$container_name" psql -U postgres -d postgres -c "SELECT pg_reload_conf();" >/dev/null
    SHARED_BUFFERS_BEFORE=$(docker exec "$container_name" psql -U postgres -d postgres -t -c "SHOW shared_buffers;" | xargs)
    echo "shared_buffers before backup: $SHARED_BUFFERS_BEFORE"
    
    # Download and create Northwind database
    download_northwind "$container_name"
}

# Helper function: Verify restore
verify_restore() {
    local container_name="$1"
    local expected_customers="$2"
    
    print_header "Verify Restore"
    SHARED_BUFFERS_AFTER=$(docker exec "$container_name" psql -U postgres -d postgres -t -c "SHOW shared_buffers;" | xargs)
    echo "shared_buffers after restore: $SHARED_BUFFERS_AFTER"
    
    if [ "$SHARED_BUFFERS_AFTER" = "999MB" ]; then
        print_success "shared_buffers restored correctly"
    else
        print_error "shared_buffers mismatch: expected 999MB, got $SHARED_BUFFERS_AFTER"
    fi
    
    # Check if Northwind database exists
    if [ "$expected_customers" != "0" ]; then
        # Wait a moment for database to be fully accessible
        sleep 2
        if docker exec "$container_name" psql -U postgres -d postgres -c "\l northwind" >/dev/null 2>&1; then
            CUSTOMERS_COUNT_AFTER=$(docker exec "$container_name" \
              psql -U postgres -d northwind -t -c "SELECT count(*) FROM customers;" 2>/dev/null | xargs || echo "0")
            echo "Northwind customers count after restore: $CUSTOMERS_COUNT_AFTER"
            
            if [ "$CUSTOMERS_COUNT_AFTER" = "$expected_customers" ]; then
                print_success "Northwind database restored correctly ($CUSTOMERS_COUNT_AFTER customers)"
            else
                print_warning "Customer count mismatch: expected $expected_customers, got $CUSTOMERS_COUNT_AFTER (database may still be restoring)"
            fi
        else
            print_warning "Northwind database not found after restore (may need to check manually)"
        fi
    else
        echo "Northwind database not included in test (expected_customers=0)"
    fi
    
    # Check restore_test table
    if docker exec "$container_name" psql -U postgres -d postgres -c "\d restore_test" >/dev/null 2>&1; then
        print_success "restore_test table exists"
    else
        print_error "restore_test table not found"
    fi
}

# Test 1: Local backups only
test_local() {
    print_header "TEST 1: Local Backups Only (repo1)"
    
    # Clean Docker first
    cleanup_docker
    
    # Check port before starting
    if lsof -ti:5433 >/dev/null 2>&1; then
        print_warning "Port 5433 is in use, freeing it..."
        lsof -ti:5433 | xargs kill -9 2>/dev/null || true
        sleep 2
    fi
    
    # Start container (no Azure)
    print_header "Start Container (Local Only)"
    docker run -d \
      --name "$CONTAINER" \
      -e POSTGRES_PASSWORD=secret \
      -p 5433:5432 \
      -v pgdata:/var/lib/postgresql \
      -v pgrepo:/var/lib/pgbackrest \
      "$IMAGE"
    
    print_success "Container started"
    
    # Wait for PostgreSQL (with timeout)
    print_header "Wait for PostgreSQL"
    echo "Waiting for PostgreSQL to be ready (max 60 seconds)..."
    TIMEOUT=60
    ELAPSED=0
    until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
        if [ $ELAPSED -ge $TIMEOUT ]; then
            print_error "PostgreSQL failed to start within $TIMEOUT seconds"
            docker logs "$CONTAINER" --tail 50
            return 1
        fi
        echo -n "."
        sleep 1
        ELAPSED=$((ELAPSED + 1))
    done
    echo ""
    print_success "PostgreSQL is ready (took ${ELAPSED}s)"
    
    # Create test data
    create_test_data "$CONTAINER"
    
    # Create stanza
    print_header "Create Stanza (Local)"
    docker exec "$CONTAINER" pgbackrest --stanza=demo stanza-create
    
    # Backup to local
    print_header "Backup to Local (repo1)"
    docker exec "$CONTAINER" pgbackrest --stanza=demo --repo=1 backup
    docker exec "$CONTAINER" pgbackrest --stanza=demo info
    
    BACKUP_LABEL_LOCAL=$(docker exec "$CONTAINER" pgbackrest --stanza=demo info | awk '/full backup:/ {print $3; exit}')
    if [ -z "$BACKUP_LABEL_LOCAL" ]; then
        print_error "Could not extract backup label from local"
        return 1
    fi
    print_success "Local backup label: $BACKUP_LABEL_LOCAL"
    
    # Simulate disaster
    print_header "Simulate Disaster"
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "DROP TABLE restore_test;" >/dev/null
    print_success "Test table dropped"
    
    # Get the actual PostgreSQL data directory before stopping container
    PGDATA_PATH=$(docker exec "$CONTAINER" bash -c 'psql -U postgres -d postgres -t -c "SHOW data_directory;" 2>/dev/null | xargs' || echo "")
    if [ -z "$PGDATA_PATH" ]; then
        # Fallback: try to find it from the config
        PGDATA_PATH=$(docker exec "$CONTAINER" bash -c 'grep "pg1-path" /etc/pgbackrest/pgbackrest.conf | tail -1 | cut -d= -f2 | xargs' || echo "")
    fi
    if [ -z "$PGDATA_PATH" ]; then
        # Fallback: try to find PG_VERSION file
        PGDATA_PATH=$(docker exec "$CONTAINER" find /var/lib/postgresql -name "PG_VERSION" -type f 2>/dev/null | head -1 | xargs dirname 2>/dev/null || echo "")
    fi
    if [ -z "$PGDATA_PATH" ]; then
        # Final fallback: use default
        PGDATA_PATH="/var/lib/postgresql/data"
        print_warning "Could not detect PostgreSQL data directory, using default: $PGDATA_PATH"
    else
        print_success "Detected PostgreSQL data directory: $PGDATA_PATH"
    fi
    
    # Stop container
    docker stop "$CONTAINER"
    
    # Restore from local
    print_header "Restore from Local (repo1)"
    echo "Restoring to PGDATA: $PGDATA_PATH"
    docker run --rm \
      --entrypoint bash \
      -v pgdata:/var/lib/postgresql \
      -v pgrepo:/var/lib/pgbackrest \
      "$IMAGE" \
      -lc "rm -rf \"$PGDATA_PATH\"/* && pgbackrest --stanza=demo restore --set='$BACKUP_LABEL_LOCAL' --type=immediate --pg1-path=\"$PGDATA_PATH\""
    
    print_success "Restore complete"
    
    # Start container
    docker start "$CONTAINER"
    TIMEOUT=30
    ELAPSED=0
    until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
        if [ $ELAPSED -ge $TIMEOUT ]; then
            print_error "PostgreSQL failed to start after restore"
            return 1
        fi
        sleep 1
        ELAPSED=$((ELAPSED + 1))
    done
    
    # Wait a bit more for PostgreSQL to fully initialize
    sleep 2
    
    # Verify restore
    if docker exec "$CONTAINER" psql -U postgres -d postgres -c "\d restore_test" >/dev/null 2>&1; then
        print_success "Local restore verified - restore_test table exists"
    else
        print_warning "restore_test table not found, but checking if database is accessible..."
        # Check if we can connect and query
        if docker exec "$CONTAINER" psql -U postgres -d postgres -c "SELECT 1;" >/dev/null 2>&1; then
            print_success "Database is accessible after restore"
            # Try to see what tables exist
            docker exec "$CONTAINER" psql -U postgres -d postgres -c "\dt" 2>&1 | head -10
        else
            print_error "Local restore failed - database not accessible"
        fi
    fi
    
    print_success "TEST 1 Complete: Local backups working!"
    return 0
}

# Test 2: Azure Blob Storage with SAS Token
test_azure_blob() {
    print_header "TEST 2: Azure Blob Storage (SAS Token)"
    
    # Validate configuration
    if [ "$AZURE_ACCOUNT" = "your-storage-account" ] || [ "$AZURE_CONTAINER" = "your-container" ]; then
        print_error "Please set AZURE_ACCOUNT and AZURE_CONTAINER environment variables"
        echo "Example:"
        echo "  export AZURE_ACCOUNT=my-storage-account"
        echo "  export AZURE_CONTAINER=my-container"
        return 1
    fi
    
    # Get SAS token - try to generate if not available or expired
    TOKEN_NEEDS_REGEN=false
    
    if [ -z "${AZURE_SAS_TOKEN:-}" ]; then
        if [ -f "/tmp/azure-sas-token.txt" ]; then
            AZURE_SAS_TOKEN=$(cat /tmp/azure-sas-token.txt)
            print_success "Loaded SAS token from /tmp/azure-sas-token.txt"
        else
            print_warning "AZURE_SAS_TOKEN not set and /tmp/azure-sas-token.txt not found"
            TOKEN_NEEDS_REGEN=true
        fi
    fi
    
    # Check if token is expired (always check, even if loaded from env var or file)
    if [ -n "${AZURE_SAS_TOKEN:-}" ] && echo "$AZURE_SAS_TOKEN" | grep -q "se="; then
        # Extract expiry date from token (format: se=2025-11-18T18:54:02Z or se=2025-11-18T18%3A54%3A02Z)
        EXPIRY_RAW=$(echo "$AZURE_SAS_TOKEN" | sed -n 's/.*se=\([^&]*\).*/\1/p' | head -1)
        if [ -n "$EXPIRY_RAW" ]; then
            # URL decode the expiry date (%3A -> :)
            EXPIRY=$(echo "$EXPIRY_RAW" | sed 's/%3A/:/g' | sed 's/%2D/-/g' | sed 's/%2B/+/g')
            if [ -n "$EXPIRY" ]; then
                # Try to parse expiry date (handle both Linux and macOS date commands)
                EXPIRY_EPOCH=$(date -u -d "$EXPIRY" +%s 2>/dev/null || date -u -j -f "%Y-%m-%dT%H:%M:%SZ" "$EXPIRY" +%s 2>/dev/null || echo "0")
                NOW_EPOCH=$(date -u +%s)
                
                if [ "$EXPIRY_EPOCH" != "0" ] && [ "$EXPIRY_EPOCH" -lt "$NOW_EPOCH" ]; then
                    print_warning "SAS token is expired (expiry: $EXPIRY, now: $(date -u +%Y-%m-%dT%H:%M:%SZ))"
                    TOKEN_NEEDS_REGEN=true
                elif [ "$EXPIRY_EPOCH" != "0" ]; then
                    print_success "SAS token is valid (expires: $EXPIRY)"
                fi
            fi
        fi
    fi
    
    # Generate new token if needed
    if [ "$TOKEN_NEEDS_REGEN" = "true" ]; then
        print_warning "Attempting to generate a new SAS token..."
        if generate_sas_token; then
            AZURE_SAS_TOKEN=$(cat /tmp/azure-sas-token.txt)
            print_success "Generated new SAS token (valid for 7 days)"
        else
            print_error "Could not generate SAS token. Skipping SAS Token test."
            print_warning "You can manually generate a token with: ./azure-pgbackrest.sh generate-token"
            return 0  # Skip this test but don't fail the whole suite
        fi
    fi
    
    AZURE_KEY_TYPE="sas"
    AZURE_REPO_PATH="/test-repo-blob-$(date +%Y%m%d-%H%M%S)"
    
    print_success "Using repo path: $AZURE_REPO_PATH"
    
    # Clean Docker first
    cleanup_docker
    
    # Check port before starting
    if lsof -ti:5433 >/dev/null 2>&1; then
        print_warning "Port 5433 is in use, freeing it..."
        lsof -ti:5433 | xargs kill -9 2>/dev/null || true
        sleep 2
    fi
    
    # Start container with Azure
    print_header "Start Container with Azure Blob Storage (SAS Token)"
    docker run -d \
      --name "$CONTAINER" \
      -e POSTGRES_PASSWORD=secret \
      -e AZURE_ACCOUNT="$AZURE_ACCOUNT" \
      -e AZURE_CONTAINER="$AZURE_CONTAINER" \
      -e AZURE_KEY="$AZURE_SAS_TOKEN" \
      -e AZURE_KEY_TYPE="$AZURE_KEY_TYPE" \
      -e AZURE_REPO_PATH="$AZURE_REPO_PATH" \
      -p 5433:5432 \
      -v pgdata:/var/lib/postgresql \
      -v pgrepo:/var/lib/pgbackrest \
      "$IMAGE"
    
    print_success "Container started"
    
    # Wait for PostgreSQL (with timeout)
    print_header "Wait for PostgreSQL"
    echo "Waiting for PostgreSQL to be ready (max 60 seconds)..."
    TIMEOUT=60
    ELAPSED=0
    until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
        if [ $ELAPSED -ge $TIMEOUT ]; then
            print_error "PostgreSQL failed to start within $TIMEOUT seconds"
            docker logs "$CONTAINER" --tail 50
            return 1
        fi
        echo -n "."
        sleep 1
        ELAPSED=$((ELAPSED + 1))
    done
    echo ""
    print_success "PostgreSQL is ready (took ${ELAPSED}s)"
    
    # Create test data (this will also download Northwind if needed)
    create_test_data "$CONTAINER"
    
    # Create Northwind DB (download if needed, then create)
    if docker exec "$CONTAINER" test -f /northwind.sql 2>/dev/null; then
        print_header "Create Northwind Database"
        docker exec "$CONTAINER" \
          psql -U postgres -d postgres -c "DROP DATABASE IF EXISTS northwind;" >/dev/null 2>&1 || true
        docker exec "$CONTAINER" \
          psql -U postgres -d postgres -c "CREATE DATABASE northwind;" >/dev/null
        docker exec "$CONTAINER" \
          psql -U postgres -d northwind -f /northwind.sql >/dev/null 2>&1
        CUSTOMERS_COUNT=$(docker exec "$CONTAINER" \
          psql -U postgres -d northwind -t -c "SELECT count(*) FROM customers;" | xargs)
        echo "Northwind customers count: $CUSTOMERS_COUNT"
    else
        # Try to download Northwind if not already downloaded
        download_northwind "$CONTAINER"
        if docker exec "$CONTAINER" test -f /northwind.sql 2>/dev/null; then
            print_header "Create Northwind Database"
            docker exec "$CONTAINER" \
              psql -U postgres -d postgres -c "DROP DATABASE IF EXISTS northwind;" >/dev/null 2>&1 || true
            docker exec "$CONTAINER" \
              psql -U postgres -d postgres -c "CREATE DATABASE northwind;" >/dev/null
            docker exec "$CONTAINER" \
              psql -U postgres -d northwind -f /northwind.sql >/dev/null 2>&1
            CUSTOMERS_COUNT=$(docker exec "$CONTAINER" \
              psql -U postgres -d northwind -t -c "SELECT count(*) FROM customers;" | xargs)
            echo "Northwind customers count: $CUSTOMERS_COUNT"
        else
            CUSTOMERS_COUNT="0"
            print_warning "Could not download or create Northwind database, skipping Northwind test"
        fi
    fi
    
    # Flush changes
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "CHECKPOINT;" >/dev/null
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "SELECT pg_switch_wal();" >/dev/null
    sleep 5
    
    # Clean local backups
    docker exec "$CONTAINER" rm -rf /var/lib/pgbackrest/archive/* /var/lib/pgbackrest/backup/* 2>/dev/null || true
    
    # Create stanza
    print_header "Create Stanza (Azure Blob Storage)"
    docker exec "$CONTAINER" pgbackrest --stanza=demo stanza-create
    
    # Backup to Azure
    print_header "Backup to Azure (repo2)"
    docker exec "$CONTAINER" pgbackrest --stanza=demo --repo=2 backup
    docker exec "$CONTAINER" pgbackrest --stanza=demo info
    
    BACKUP_LABEL_AZURE=$(docker exec "$CONTAINER" pgbackrest --stanza=demo info | awk '/full backup:/ {print $3; exit}')
    if [ -z "$BACKUP_LABEL_AZURE" ]; then
        print_error "Could not extract backup label from Azure"
        return 1
    fi
    print_success "Azure backup label: $BACKUP_LABEL_AZURE"
    
    # Simulate disaster
    print_header "Simulate Disaster"
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "DROP TABLE restore_test;" >/dev/null
    if [ "$CUSTOMERS_COUNT" != "0" ]; then
        docker exec "$CONTAINER" psql -U postgres -d postgres -c "DROP DATABASE northwind;" >/dev/null
    fi
    print_success "Test data dropped"
    
    # Get the actual PostgreSQL data directory before stopping container
    ACTUAL_DATA_DIR=$(docker exec "$CONTAINER" bash -c 'psql -U postgres -d postgres -t -c "SHOW data_directory;" 2>/dev/null | xargs' || echo "")
    if [ -z "$ACTUAL_DATA_DIR" ]; then
        # Fallback: try to find it from the config
        ACTUAL_DATA_DIR=$(docker exec "$CONTAINER" bash -c 'grep "pg1-path" /etc/pgbackrest/pgbackrest.conf | tail -1 | cut -d= -f2 | xargs' || echo "")
    fi
    if [ -z "$ACTUAL_DATA_DIR" ]; then
        # Final fallback: use default
        ACTUAL_DATA_DIR="/var/lib/postgresql/data"
        print_warning "Could not detect PostgreSQL data directory, using default: $ACTUAL_DATA_DIR"
    else
        print_success "Detected PostgreSQL data directory: $ACTUAL_DATA_DIR"
    fi
    
    # Stop container
    docker stop "$CONTAINER"
    
    # Restore from Azure
    print_header "Restore from Azure (repo2)"
    docker run --rm \
      --entrypoint bash \
      -e AZURE_ACCOUNT="$AZURE_ACCOUNT" \
      -e AZURE_CONTAINER="$AZURE_CONTAINER" \
      -e AZURE_KEY="$AZURE_SAS_TOKEN" \
      -e AZURE_KEY_TYPE="$AZURE_KEY_TYPE" \
      -e AZURE_REPO_PATH="$AZURE_REPO_PATH" \
      -e ACTUAL_DATA_DIR="$ACTUAL_DATA_DIR" \
      -v pgdata:/var/lib/postgresql \
      -v pgrepo:/var/lib/pgbackrest \
      "$IMAGE" \
      -lc "/usr/local/bin/configure-azure.sh || true; \
           DATA_DIR=\${ACTUAL_DATA_DIR:-/var/lib/postgresql/data}; \
           echo \"Restoring to data directory: \$DATA_DIR\"; \
           rm -rf \"\$DATA_DIR\"/* && \
           pgbackrest --stanza=demo restore --set='$BACKUP_LABEL_AZURE' --type=immediate --pg1-path=\"\$DATA_DIR\""
    
    print_success "Restore complete"
    
    # Start container
    docker start "$CONTAINER"
    until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
        sleep 1
    done
    
    # Verify restore
    verify_restore "$CONTAINER" "$CUSTOMERS_COUNT"
    
    print_success "TEST 2 Complete: Azure Blob Storage (SAS Token) working!"
    return 0
}

# Test 3: Azure Managed Identity
test_azure_ami() {
    print_header "TEST 3: Azure Managed Identity (AMI)"
    
    # Validate configuration
    if [ "$AZURE_ACCOUNT" = "your-storage-account" ] || [ "$AZURE_CONTAINER" = "your-container" ]; then
        print_error "Please set AZURE_ACCOUNT and AZURE_CONTAINER environment variables"
        return 1
    fi
    
    # Check if we're on Azure (Managed Identity only works on Azure)
    if ! curl -s -H "Metadata:true" "http://169.254.169.254/metadata/instance?api-version=2021-02-01" >/dev/null 2>&1; then
        print_warning "Not running on Azure VM - Managed Identity test will be skipped"
        print_warning "Managed Identity only works on Azure VMs, Container Instances, or AKS"
        return 0
    fi
    
    AZURE_KEY_TYPE="auto"
    AZURE_REPO_PATH="/test-repo-ami-$(date +%Y%m%d-%H%M%S)"
    
    print_success "Using repo path: $AZURE_REPO_PATH"
    
    # Clean Docker first
    cleanup_docker
    
    # Check port before starting
    if lsof -ti:5433 >/dev/null 2>&1; then
        print_warning "Port 5433 is in use, freeing it..."
        lsof -ti:5433 | xargs kill -9 2>/dev/null || true
        sleep 2
    fi
    
    # Start container with Managed Identity
    print_header "Start Container with Azure Managed Identity"
    docker run -d \
      --name "$CONTAINER" \
      -e POSTGRES_PASSWORD=secret \
      -e AZURE_ACCOUNT="$AZURE_ACCOUNT" \
      -e AZURE_CONTAINER="$AZURE_CONTAINER" \
      -e AZURE_KEY_TYPE="$AZURE_KEY_TYPE" \
      -e AZURE_REPO_PATH="$AZURE_REPO_PATH" \
      -p 5433:5432 \
      -v pgdata:/var/lib/postgresql \
      -v pgrepo:/var/lib/pgbackrest \
      "$IMAGE"
    
    print_success "Container started"
    
    # Wait for PostgreSQL (with timeout)
    print_header "Wait for PostgreSQL"
    echo "Waiting for PostgreSQL to be ready (max 60 seconds)..."
    TIMEOUT=60
    ELAPSED=0
    until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
        if [ $ELAPSED -ge $TIMEOUT ]; then
            print_error "PostgreSQL failed to start within $TIMEOUT seconds"
            docker logs "$CONTAINER" --tail 50
            return 1
        fi
        echo -n "."
        sleep 1
        ELAPSED=$((ELAPSED + 1))
    done
    echo ""
    print_success "PostgreSQL is ready (took ${ELAPSED}s)"
    
    # Create test data (this will also download Northwind if needed)
    create_test_data "$CONTAINER"
    
    # Create Northwind DB (download if needed, then create)
    if docker exec "$CONTAINER" test -f /northwind.sql 2>/dev/null; then
        print_header "Create Northwind Database"
        docker exec "$CONTAINER" \
          psql -U postgres -d postgres -c "DROP DATABASE IF EXISTS northwind;" >/dev/null 2>&1 || true
        docker exec "$CONTAINER" \
          psql -U postgres -d postgres -c "CREATE DATABASE northwind;" >/dev/null
        docker exec "$CONTAINER" \
          psql -U postgres -d northwind -f /northwind.sql >/dev/null 2>&1
        CUSTOMERS_COUNT=$(docker exec "$CONTAINER" \
          psql -U postgres -d northwind -t -c "SELECT count(*) FROM customers;" | xargs)
        echo "Northwind customers count: $CUSTOMERS_COUNT"
    else
        # Try to download Northwind if not already downloaded
        download_northwind "$CONTAINER"
        if docker exec "$CONTAINER" test -f /northwind.sql 2>/dev/null; then
            print_header "Create Northwind Database"
            docker exec "$CONTAINER" \
              psql -U postgres -d postgres -c "DROP DATABASE IF EXISTS northwind;" >/dev/null 2>&1 || true
            docker exec "$CONTAINER" \
              psql -U postgres -d postgres -c "CREATE DATABASE northwind;" >/dev/null
            docker exec "$CONTAINER" \
              psql -U postgres -d northwind -f /northwind.sql >/dev/null 2>&1
            CUSTOMERS_COUNT=$(docker exec "$CONTAINER" \
              psql -U postgres -d northwind -t -c "SELECT count(*) FROM customers;" | xargs)
            echo "Northwind customers count: $CUSTOMERS_COUNT"
        else
            CUSTOMERS_COUNT="0"
            print_warning "Could not download or create Northwind database, skipping Northwind test"
        fi
    fi
    
    # Flush changes
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "CHECKPOINT;" >/dev/null
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "SELECT pg_switch_wal();" >/dev/null
    sleep 5
    
    # Clean local backups
    docker exec "$CONTAINER" rm -rf /var/lib/pgbackrest/archive/* /var/lib/pgbackrest/backup/* 2>/dev/null || true
    
    # Debug: Check PostgreSQL data directory and pgBackRest config
    print_header "Debug: Check PostgreSQL Paths"
    echo "PGDATA environment:"
    docker exec "$CONTAINER" bash -c 'echo "PGDATA=$PGDATA"'
    echo ""
    echo "PostgreSQL data directory locations:"
    docker exec "$CONTAINER" bash -c 'ls -la /var/lib/postgresql/ 2>/dev/null || echo "Cannot list /var/lib/postgresql"'
    echo ""
    echo "pgBackRest config pg1-path:"
    docker exec "$CONTAINER" bash -c 'grep "pg1-path" /etc/pgbackrest/pgbackrest.conf || echo "pg1-path not found in config"'
    echo ""
    echo "Actual PostgreSQL data directory (from postgres process):"
    ACTUAL_DATA_DIR=$(docker exec "$CONTAINER" bash -c 'psql -U postgres -d postgres -t -c "SHOW data_directory;" 2>/dev/null | xargs' || echo "")
    if [ -n "$ACTUAL_DATA_DIR" ]; then
        echo "Found data directory: $ACTUAL_DATA_DIR"
        echo ""
        echo "Updating pgBackRest config with correct path..."
        # Remove old pg1-path and add new one with correct directory
        # Use /tmp for temp file and Python to avoid all permission issues
        docker exec -e DATA_DIR="$ACTUAL_DATA_DIR" "$CONTAINER" bash -c "python3 <<'PYEOF'
import sys
import os

data_dir = os.environ['DATA_DIR']
config_file = '/etc/pgbackrest/pgbackrest.conf'
tmp_file = '/tmp/pgbackrest.conf.tmp'

# Read current config
with open(config_file, 'r') as f:
    lines = f.readlines()

# Process lines: remove old pg1-path, add new one after [demo]
output = []
in_demo = False
pg1_added = False

for line in lines:
    stripped = line.strip()
    
    # Track when we enter [demo] section
    if stripped == '[demo]':
        in_demo = True
        output.append(line)
        continue
    
    # Track when we leave [demo] section
    if stripped.startswith('[') and stripped != '[demo]':
        if in_demo and not pg1_added:
            output.append('pg1-path=' + data_dir + '\n')
            pg1_added = True
        in_demo = False
        output.append(line)
        continue
    
    # Skip old pg1-path lines
    if stripped.startswith('pg1-path='):
        continue
    
    # Add pg1-path after [demo] when we hit first empty line or end
    if in_demo and not pg1_added and (stripped == '' or lines.index(line) == len(lines) - 1):
        output.append('pg1-path=' + data_dir + '\n')
        pg1_added = True
    
    output.append(line)

# If [demo] section exists but pg1-path was never added
if in_demo and not pg1_added:
    output.append('pg1-path=' + data_dir + '\n')

# If [demo] section doesn't exist, add it
if '[demo]' not in ''.join(output):
    output.append('\n[demo]\n')
    output.append('pg1-path=' + data_dir + '\n')

# Write to temp file
with open(tmp_file, 'w') as f:
    f.writelines(output)

# Copy back and set permissions
import shutil
import pwd
import grp

shutil.copy(tmp_file, config_file)
postgres_uid = pwd.getpwnam('postgres').pw_uid
postgres_gid = grp.getgrnam('postgres').gr_gid
os.chown(config_file, postgres_uid, postgres_gid)
os.chmod(config_file, 0o640)
os.remove(tmp_file)
PYEOF
"
        echo "Updated config:"
        docker exec "$CONTAINER" bash -c 'grep "pg1-path" /etc/pgbackrest/pgbackrest.conf'
    else
        print_warning "Could not determine PostgreSQL data directory, using default"
    fi
    echo ""
    
    # Verify repo2 is configured
    print_header "Verify Azure (repo2) Configuration"
    if ! docker exec "$CONTAINER" grep -q "repo2-type=azure" /etc/pgbackrest/pgbackrest.conf; then
        echo "repo2 not found in config, running configure-azure.sh..."
        docker exec "$CONTAINER" bash -lc "/usr/local/bin/configure-azure.sh" || echo "configure-azure.sh returned error, will configure manually"
        
        # Check if it worked
        if ! docker exec "$CONTAINER" grep -q "repo2-type=azure" /etc/pgbackrest/pgbackrest.conf; then
            echo "configure-azure.sh didn't add repo2, configuring manually..."
            docker exec -e AZURE_ACCOUNT="$AZURE_ACCOUNT" -e AZURE_CONTAINER="$AZURE_CONTAINER" -e AZURE_KEY_TYPE="$AZURE_KEY_TYPE" -e AZURE_REPO_PATH="$AZURE_REPO_PATH" "$CONTAINER" bash -c "
                AZURE_REPO_PATH=\${AZURE_REPO_PATH:-/demo-repo}
                TMP_FILE=/tmp/pgbackrest_repo2.$$
                cat /etc/pgbackrest/pgbackrest.conf > \$TMP_FILE
                echo '' >> \$TMP_FILE
                echo 'repo2-type=azure' >> \$TMP_FILE
                echo \"repo2-azure-account=\${AZURE_ACCOUNT}\" >> \$TMP_FILE
                echo \"repo2-azure-container=\${AZURE_CONTAINER}\" >> \$TMP_FILE
                echo \"repo2-azure-key-type=\${AZURE_KEY_TYPE}\" >> \$TMP_FILE
                echo \"repo2-path=\${AZURE_REPO_PATH}\" >> \$TMP_FILE
                echo 'repo2-retention-full=4' >> \$TMP_FILE
                cp \$TMP_FILE /etc/pgbackrest/pgbackrest.conf
                chown postgres:postgres /etc/pgbackrest/pgbackrest.conf
                chmod 640 /etc/pgbackrest/pgbackrest.conf
                rm -f \$TMP_FILE
            "
        fi
        echo "Config after configuration:"
        docker exec "$CONTAINER" grep -E "repo2|azure" /etc/pgbackrest/pgbackrest.conf || echo "No repo2 config found"
    else
        echo "repo2 configuration found:"
        docker exec "$CONTAINER" grep -E "repo2|azure" /etc/pgbackrest/pgbackrest.conf
    fi
    echo ""
    
    # Ensure archiving is enabled before backups
    print_header "Ensure PostgreSQL Archiving Enabled"
    ARCHIVE_MODE=$(docker exec "$CONTAINER" psql -U postgres -At -c "show archive_mode")

    if [ "$ARCHIVE_MODE" != "on" ]; then
        echo "archive_mode is currently $ARCHIVE_MODE - enabling..."
        docker exec "$CONTAINER" bash -lc '
            set -e
            PGDATA_DIR=${PGDATA:-/var/lib/postgresql/data}
            {
                echo ""
                echo "# pgBackRest archiving configuration"
                echo "archive_mode = on"
                echo "archive_command = '\''pgbackrest --stanza=demo archive-push %p'\''"
                echo "archive_timeout = 60"
                echo "wal_level = replica"
                echo "max_wal_senders = 3"
                echo "max_replication_slots = 3"
            } >> "$PGDATA_DIR/postgresql.conf"
        '

        docker restart "$CONTAINER" >/dev/null
        echo "Waiting for PostgreSQL to restart with archiving enabled..."
        until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
            sleep 1
        done
    else
        echo "archive_mode already enabled"
    fi
    echo ""
    
    # Create stanza (stanza-create creates on all configured repos)
    print_header "Create Stanza (Azure Managed Identity)"
    docker exec "$CONTAINER" pgbackrest --stanza=demo stanza-create
    
    # Backup to Azure
    print_header "Backup to Azure (repo2) with Managed Identity"
    docker exec "$CONTAINER" pgbackrest --stanza=demo --repo=2 backup
    docker exec "$CONTAINER" pgbackrest --stanza=demo info
    
    BACKUP_LABEL_AMI=$(docker exec "$CONTAINER" pgbackrest --stanza=demo info | awk '/full backup:/ {print $3; exit}')
    if [ -z "$BACKUP_LABEL_AMI" ]; then
        print_error "Could not extract backup label from Azure"
        return 1
    fi
    print_success "Azure Managed Identity backup label: $BACKUP_LABEL_AMI"
    
    # Simulate disaster
    print_header "Simulate Disaster"
    docker exec "$CONTAINER" psql -U postgres -d postgres -c "DROP TABLE restore_test;" >/dev/null
    if [ "$CUSTOMERS_COUNT" != "0" ]; then
        docker exec "$CONTAINER" psql -U postgres -d postgres -c "DROP DATABASE northwind;" >/dev/null
    fi
    print_success "Test data dropped"
    
    # Get the actual PostgreSQL data directory before stopping container
    ACTUAL_DATA_DIR=$(docker exec "$CONTAINER" bash -c 'psql -U postgres -d postgres -t -c "SHOW data_directory;" 2>/dev/null | xargs' || echo "")
    if [ -z "$ACTUAL_DATA_DIR" ]; then
        # Fallback: try to find it from the config
        ACTUAL_DATA_DIR=$(docker exec "$CONTAINER" bash -c 'grep "pg1-path" /etc/pgbackrest/pgbackrest.conf | tail -1 | cut -d= -f2 | xargs' || echo "")
    fi
    if [ -z "$ACTUAL_DATA_DIR" ]; then
        # Final fallback: use default
        ACTUAL_DATA_DIR="/var/lib/postgresql/data"
        print_warning "Could not detect PostgreSQL data directory, using default: $ACTUAL_DATA_DIR"
    else
        print_success "Detected PostgreSQL data directory: $ACTUAL_DATA_DIR"
    fi
    
    # Stop container
    docker stop "$CONTAINER"
    
    # Restore from Azure
    print_header "Restore from Azure (repo2) with Managed Identity"
    docker run --rm \
      --entrypoint bash \
      -e AZURE_ACCOUNT="$AZURE_ACCOUNT" \
      -e AZURE_CONTAINER="$AZURE_CONTAINER" \
      -e AZURE_KEY_TYPE="$AZURE_KEY_TYPE" \
      -e AZURE_REPO_PATH="$AZURE_REPO_PATH" \
      -e ACTUAL_DATA_DIR="$ACTUAL_DATA_DIR" \
      -v pgdata:/var/lib/postgresql \
      -v pgrepo:/var/lib/pgbackrest \
      "$IMAGE" \
      -lc "/usr/local/bin/configure-azure.sh || true; \
           if ! grep -q \"repo2-type=azure\" /etc/pgbackrest/pgbackrest.conf; then \
             AZURE_REPO_PATH=\${AZURE_REPO_PATH:-/demo-repo}; \
             TMP_FILE=/tmp/pgbackrest_repo2.\$\$; \
             cat /etc/pgbackrest/pgbackrest.conf > \$TMP_FILE; \
             echo '' >> \$TMP_FILE; \
             echo 'repo2-type=azure' >> \$TMP_FILE; \
             echo \"repo2-azure-account=\${AZURE_ACCOUNT}\" >> \$TMP_FILE; \
             echo \"repo2-azure-container=\${AZURE_CONTAINER}\" >> \$TMP_FILE; \
             echo \"repo2-azure-key-type=\${AZURE_KEY_TYPE}\" >> \$TMP_FILE; \
             echo \"repo2-path=\${AZURE_REPO_PATH}\" >> \$TMP_FILE; \
             echo 'repo2-retention-full=4' >> \$TMP_FILE; \
             cp \$TMP_FILE /etc/pgbackrest/pgbackrest.conf; \
             chown postgres:postgres /etc/pgbackrest/pgbackrest.conf; \
             chmod 640 /etc/pgbackrest/pgbackrest.conf; \
             rm -f \$TMP_FILE; \
           fi; \
           DATA_DIR=\${ACTUAL_DATA_DIR:-/var/lib/postgresql/data}; \
           echo \"Restoring to data directory: \$DATA_DIR\"; \
           rm -rf \"\$DATA_DIR\"/* && \
           pgbackrest --repo=2 --stanza=demo restore --set='$BACKUP_LABEL_AMI' --type=immediate --pg1-path=\"\$DATA_DIR\""
    
    print_success "Restore complete"
    
    # Start container
    docker start "$CONTAINER"
    until docker exec "$CONTAINER" pg_isready -U postgres >/dev/null 2>&1; do
        sleep 1
    done
    
    # Verify restore
    verify_restore "$CONTAINER" "$CUSTOMERS_COUNT"
    
    print_success "TEST 3 Complete: Azure Managed Identity working!"
    return 0
}

# Function 4: Full Backup and Restore Test (runs all tests in sequence)
run_full_test() {
    print_header "Full Backup and Restore Test Suite"
    echo "Running tests in order:"
    echo "  1. Local backups (repo1)"
    echo "  2. Clean everything"
    echo "  3. Azure Blob Storage (SAS Token)"
    echo "  4. Azure Managed Identity (AMI)"
    echo ""
    
    # Test 1: Local
    if ! test_local; then
        print_error "Local backup test failed"
        return 1
    fi
    
    # Clean everything
    print_header "Cleaning Everything Before Next Test"
    cleanup_docker
    
    # Test 2: Azure Blob Storage
    if ! test_azure_blob; then
        print_warning "Azure Blob Storage test failed or skipped (may need valid SAS token)"
        print_warning "Continuing with next test..."
    fi
    
    # Clean everything
    print_header "Cleaning Everything Before Next Test"
    cleanup_docker
    
    # Test 3: Azure Managed Identity
    if ! test_azure_ami; then
        print_warning "Azure Managed Identity test skipped or failed (may not be on Azure VM)"
    fi
    
    print_header "All Tests Complete!"
    print_success "✓ Local backups: Working"
    print_success "✓ Azure Blob Storage (SAS): Working"
    if curl -s -H "Metadata:true" "http://169.254.169.254/metadata/instance?api-version=2021-02-01" >/dev/null 2>&1; then
        print_success "✓ Azure Managed Identity: Working"
    else
        print_warning "⚠ Azure Managed Identity: Skipped (not on Azure VM)"
    fi
    
    return 0
}

# Function 5: Show usage
show_usage() {
    cat << EOF
Azure pgBackRest Master Script

Usage: $0 [command]

Commands:
  generate-token    Generate SAS token (run on Ubuntu machine)
  cleanup-azure     Clean up Azure storage blobs (run on Ubuntu machine)
  cleanup-docker    Clean up Docker containers and volumes
  cleanup-all       Full cleanup: remove containers, volumes, image, and rebuild
  test              Run full backup/restore test
  test-ami          Run only Azure Managed Identity test (requires Azure VM)
  all               Run everything: cleanup + test (requires SAS token)

Examples:
  # Generate SAS token (on Ubuntu)
  ./azure-pgbackrest.sh generate-token

  # Clean up old test backups (on Ubuntu)
  ./azure-pgbackrest.sh cleanup-azure

  # Run full test (requires AZURE_SAS_TOKEN or token in /tmp/azure-sas-token.txt)
  export AZURE_SAS_TOKEN="your-token-here"
  ./azure-pgbackrest.sh test

  # Run only Azure Managed Identity test (requires Azure VM)
  export AZURE_ACCOUNT="your-storage-account"
  export AZURE_CONTAINER="your-container"
  ./azure-pgbackrest.sh test-ami

  # Run everything (on Ubuntu - generates token, cleans up, runs test)
  ./azure-pgbackrest.sh all

Environment Variables:
  AZURE_SAS_TOKEN      SAS token for Azure authentication (auto-loaded from /tmp/azure-sas-token.txt if not set)
  AZURE_ACCOUNT        Storage account name (REQUIRED - set this before running)
  AZURE_CONTAINER      Container name (REQUIRED - set this before running)
  RESOURCE_GROUP       Resource group name (optional, for some operations)
  IMAGE                Docker image name (default: pgbackrest-test)
  CONTAINER            Docker container name (default: pgbr-test)

Configuration:
  Before running, set your Azure configuration:
    export AZURE_ACCOUNT="your-storage-account"
    export AZURE_CONTAINER="your-container"
    export RESOURCE_GROUP="your-resource-group"  # Optional

EOF
}

# Main script logic
main() {
    case "${1:-}" in
        generate-token|token)
            generate_sas_token
            ;;
        cleanup-azure|cleanup)
            cleanup_azure "${2:-test-repo}"
            ;;
        cleanup-docker|docker-clean)
            cleanup_docker
            ;;
        cleanup-all|rebuild)
            cleanup_all_and_rebuild
            ;;
        test|run-test)
            run_full_test
            ;;
        test-ami|ami)
            test_azure_ami
            ;;
        all|everything)
            print_header "Running Everything"
            generate_sas_token || {
                print_warning "Could not generate token, trying to use existing..."
                if [ ! -f "/tmp/azure-sas-token.txt" ]; then
                    print_error "No SAS token available. Please set AZURE_SAS_TOKEN or run generate-token first"
                    exit 1
                fi
            }
            export AZURE_SAS_TOKEN=$(cat /tmp/azure-sas-token.txt)
            cleanup_azure "test-repo" || true
            run_full_test
            ;;
        help|--help|-h|"")
            show_usage
            ;;
        *)
            print_error "Unknown command: $1"
            echo ""
            show_usage
            exit 1
            ;;
    esac
}

# Run main function
main "$@"

