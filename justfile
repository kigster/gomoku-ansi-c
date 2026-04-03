# Gomoku Project
# Compilation is handled by Makefile; this file handles everything else.

set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

version := `grep 'GAME_VERSION' src/gomoku/gomoku.h | awk '{print $3}' | tr -d '"'`
tag     := "v" + version

[no-exit-message]
recipes:
    @just --choose

# ─── Build ────────────────────────────────────────────────────────────────────

# Build all binaries (game, daemon, test client)
build:
    make all

# Clean and rebuild the game binary
rebuild:
    make rebuild

# Clean all build artifacts
clean:
    make clean

# ─── Test ─────────────────────────────────────────────────────────────────────

# Run all unit tests (game + daemon)
test:
    make test

# Run daemon unit tests only
test-daemon:
    make test-daemon

# ─── Version & Release ────────────────────────────────────────────────────────

# Print the current version and tag
version:
    @echo "Version is {{ version }}"
    @echo "The tag is {{ tag }}"

# Tag the current commit with the version
tag:
    git tag -f {{ tag }} -m {{ tag }} && git push --tags --force || true

# Create a GitHub release from the current version tag
release: tag
    gh release create {{ tag }} --generate-notes

# ─── Code Quality ─────────────────────────────────────────────────────────────

# Format all source, test, and script files
format:
    find src/gomoku src/net -maxdepth 1 -name '*.c**' | xargs clang-format -i
    find tests -maxdepth 1 -name '*.c**' | xargs clang-format -i
    find bin -type f -exec bash -c 'file {} | grep -Eqvi ruby' \; -print | xargs shfmt -i 2 -w

# Validate config/sample-game.json against the JSON schema
validate-json:
    bundle check >/dev/null || bundle install -j 8
    TERM=xterm-256color bundle exec bin/schema-validator validate-json

# ─── Ruby ─────────────────────────────────────────────────────────────────────

# Install Ruby dependencies
bundle:
    #!/usr/bin/env bash
    set -euo pipefail
    if [ -d ~/.rbenv/plugins/ruby-build ]; then
        cd ~/.rbenv/plugins/ruby-build
        git pull --rebase || true
    fi
    command -v rbenv >/dev/null && rbenv install -s "$(cat .ruby-version | tr -d '\n')"
    bundle install -j 4

# ─── Install ──────────────────────────────────────────────────────────────────

# Install binaries to prefix (default: /usr/local)
install prefix="/usr/local": build
    install -d {{ prefix }}/bin
    install -m 755 gomoku gomoku-httpd gomoku-http-client bin/gomoku-httpd-ctl {{ prefix }}/bin

# Uninstall the gomoku binary from prefix
uninstall prefix="/usr/local":
    rm -f {{ prefix }}/bin/gomoku

# ─── AI Evaluations ──────────────────────────────────────────────────────────

# Run all AI evaluation scripts (tactical + tournament)
evals: build
    #!/usr/bin/env bash
    set -uo pipefail
    echo "=== Running Tactical Tests ==="
    chmod +x tests/evals/bash/run-tactical-tests
    tests/evals/bash/run-tactical-tests || true
    echo ""
    echo "=== Running Depth Tournament ==="
    chmod +x tests/evals/bash/depth-tournament
    tests/evals/bash/depth-tournament --games 10 --depths "2,3,4"

# Run tactical position tests
eval-tactical: build
    #!/usr/bin/env bash
    set -uo pipefail
    echo "=== Running Tactical Tests ==="
    chmod +x tests/evals/bash/run-tactical-tests
    tests/evals/bash/run-tactical-tests || true

# Run depth tournament (AI vs AI at different depths)
eval-tournament: build
    @echo "=== Running Depth Tournament ==="
    chmod +x tests/evals/bash/depth-tournament
    tests/evals/bash/depth-tournament --games 10 --depths "2,3,4"

# Run LLM-based game evaluation (requires ANTHROPIC_API_KEY)
eval-llm: build
    @echo "=== Running LLM Evaluation ==="
    uv run tests/evals/python/llm_eval.py

# Run bash depth tournament with custom params
evals-bash:
    tests/evals/bash/depth-tournament -d 1,2,3,4,5 -r 3,4 --games 10

# Run ruby tournament against gomoku-httpd cluster behind envoy
evals-ruby:
    #!/usr/bin/env bash
    set -euo pipefail
    echo "Starting gomoku-httpd cluster behind envoy..."
    (gctl ps | grep -q -E 'gomoku-httpd' && gctl ps | grep -q -E 'envoy') && \
        echo "Cluster is already up :)" || gctl start -p envoy
    cd tests/evals/ruby
    bundle check || bundle install -j 12
    ln -nfs ../../../gomoku-http-client .
    bundle exec depth-tournament tournament -d 1,2,3,4 -r 2,3 --games 5 --verbose

# ─── Docker ───────────────────────────────────────────────────────────────────

# Build the gomoku-httpd docker container
docker-build:
    docker build -t gomoku-httpd:latest .

# Build the gomoku-frontend docker container
docker-build-frontend:
    docker build -t gomoku-frontend:latest frontend/

# Build the gomoku-api docker container
docker-build-api:
    docker build -t gomoku-api:latest api/

# Build all docker containers
docker-build-all: docker-build docker-build-frontend docker-build-api

# Build gomoku-httpd for linux/amd64 (for GCP)
docker-build-amd64:
    docker buildx build --platform linux/amd64 -t gomoku-httpd:latest --load .

# Build gomoku-frontend for linux/amd64 (for GCP)
docker-build-frontend-amd64:
    docker buildx build --platform linux/amd64 -t gomoku-frontend:latest --load frontend/

# Build gomoku-api for linux/amd64 (for GCP)
docker-build-api-amd64:
    docker buildx build --platform linux/amd64 -t gomoku-api:latest --load api/

# Build all containers for linux/amd64
docker-build-all-amd64: docker-build-amd64 docker-build-frontend-amd64 docker-build-api-amd64

# Run the gomoku-httpd docker container
docker-run:
    docker run -p 8787:8787 gomoku-httpd:latest

# ─── Cloud Run ────────────────────────────────────────────────────────────────

# Deploy to Cloud Run for the first time
cr-init: docker-build-all-amd64
    #!/usr/bin/env bash
    set -euo pipefail
    echo "Initial deploy..."
    gcloud auth application-default login
    cd ./iac/cloud_run && bash deploy.sh

# Update Cloud Run with the latest code
cr-update: docker-build-all-amd64
    #!/usr/bin/env bash
    set -euo pipefail
    gcloud auth application-default login
    cd ./iac/cloud_run && bash update.sh

# ─── CMake ────────────────────────────────────────────────────────────────────

# Build using CMake
cmake-build:
    mkdir -p build
    cd build && cmake .. && make

# Clean CMake build directory
cmake-clean:
    make cmake-clean

# Run tests using CMake
cmake-test: cmake-build
    cd build && ctest --verbose

# Clean and rebuild using CMake
cmake-rebuild: cmake-clean cmake-build
