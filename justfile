# Gomoku Monorepo
# C engine: gomoku-c/Makefile — API: api/ — Frontend: frontend/

set shell := ["bash", "-eu", "-o", "pipefail", "-c"]

version := `grep 'GAME_VERSION' gomoku-c/src/gomoku/gomoku.h | awk '{print $3}' | tr -d '"'| tr -d '\n'`
tag     := "v" + version

[no-exit-message]
recipes:
    @just --choose

# ─── Build ────────────────────────────────────────────────────────────────────

# Build terminal game only (no frontend/API dependencies)
build-game:
    make -C gomoku-c all install

# Build everything: C engine + frontend + copy assets to api/public
build: install-frontend
    make -C gomoku-c all install

# Clean and rebuild the game binary
rebuild:
    make -C gomoku-c rebuild

# Clean all build artifacts
clean:
    make -C gomoku-c clean
    find . -maxdepth 1 -type f -name 'gomoku*' -delete

# Build frontend static assets into frontend/dist
build-frontend:
    cd frontend && npm run build

# Copy frontend dist into API public directory
install-frontend: build-frontend
    rm -rf api/public
    cp -r frontend/dist api/public

# ─── Test ─────────────────────────────────────────────────────────────────────

# Run C engine unit tests (game + daemon)
test: test-daemon test-api test-frontend
    make -C gomoku-c test

test-gomoku-c: 
    make -C gmoku-c test

# Run daemon unit tests only
test-daemon:
    make -C gomoku-c test-daemon

# Run API tests
test-api:
    cd api && just install && just test

# Run frontend tests
test-frontend:
    cd frontend && npm test

# Run all tests across the monorepo
test-all: test test-api test-frontend

# Run all pre-commit tests and linters
ci:
    @lefthook run --all-files pre-commit

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
    find gomoku-c/src/gomoku gomoku-c/src/net -maxdepth 1 -name '*.c**' | xargs clang-format -i
    find gomoku-c/tests -maxdepth 1 -name '*.c**' | xargs clang-format -i
    find bin -type f -exec bash -c 'file {} | grep -Eqvi ruby' \; -print | xargs shfmt -i 2 -w

# Validate config/sample-game.json against the JSON schema
validate-json:
    cd schema-validator && bundle check >/dev/null || bundle install -j 8
    cd schema-validator && TERM=xterm-256color bundle exec bin/schema-validator validate-json

# ─── AI Evaluations ──────────────────────────────────────────────────────────

# Run all AI evaluation scripts (tactical + tournament)
evals: build
    #!/usr/bin/env bash
    set -uo pipefail
    echo "=== Running Tactical Tests ==="
    chmod +x gomoku-c/tests/evals/bash/run-tactical-tests
    gomoku-c/tests/evals/bash/run-tactical-tests || true
    echo ""
    echo "=== Running Depth Tournament ==="
    chmod +x gomoku-c/tests/evals/bash/depth-tournament
    gomoku-c/tests/evals/bash/depth-tournament --games 10 --depths "2,3,4"

# Run tactical position tests
eval-tactical: build
    #!/usr/bin/env bash
    set -uo pipefail
    echo "=== Running Tactical Tests ==="
    chmod +x gomoku-c/tests/evals/bash/run-tactical-tests
    gomoku-c/tests/evals/bash/run-tactical-tests || true

# Run depth tournament (AI vs AI at different depths)
eval-tournament: build
    @echo "=== Running Depth Tournament ==="
    chmod +x gomoku-c/tests/evals/bash/depth-tournament
    gomoku-c/tests/evals/bash/depth-tournament --games 10 --depths "2,3,4"

# Run LLM-based game evaluation (requires ANTHROPIC_API_KEY)
eval-llm: build
    @echo "=== Running LLM Evaluation ==="
    uv run gomoku-c/tests/evals/python/llm_eval.py

# Run bash depth tournament with custom params
evals-bash:
    gomoku-c/tests/evals/bash/depth-tournament -d 1,2,3,4,5 -r 3,4 --games 10

# Run ruby tournament against gomoku-httpd cluster behind envoy
evals-ruby:
    #!/usr/bin/env bash
    set -euo pipefail
    echo "Starting gomoku-httpd cluster behind envoy..."
    (gctl ps | grep -q -E 'gomoku-httpd' && gctl ps | grep -q -E 'envoy') && \
        echo "Cluster is already up :)" || gctl start -p envoy
    cd gomoku-c/tests/evals/ruby
    bundle check || bundle install -j 12
    ln -nfs ../../bin/gomoku-http-client .
    bundle exec depth-tournament tournament -d 1,2,3,4 -r 2,3 --games 5 --verbose

# ─── Docker ───────────────────────────────────────────────────────────────────

# Build the gomoku-httpd docker container
docker-build-httpd:
    docker build -t gomoku-httpd:latest gomoku-c/

# Build the gomoku-api docker container (includes frontend static assets)
docker-build-api: install-frontend
    docker build -t gomoku-api:latest api/

# Build all docker containers
docker-build-all: docker-build-httpd docker-build-api

# Build gomoku-httpd for linux/amd64 (for GCP)
docker-build-httpd-amd64:
    docker buildx build --platform linux/amd64 -t gomoku-httpd:latest --load gomoku-c/

# Build gomoku-api for linux/amd64 (for GCP, includes frontend)
docker-build-api-amd64: install-frontend
    docker buildx build --platform linux/amd64 -t gomoku-api:latest --load api/

# Build all containers for linux/amd64
docker-build-all-amd64: docker-build-httpd-amd64 docker-build-api-amd64

# Build everything and prepare Docker images for Cloud Run deploy
cr-prepare: docker-build-all-amd64

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
    mkdir -p gomoku-c/build
    cd gomoku-c/build && cmake .. && make

# Clean CMake build directory
cmake-clean:
    make -C gomoku-c cmake-clean

# Run tests using CMake
cmake-test: cmake-build
    cd gomoku-c/build && ctest --verbose

# Clean and rebuild using CMake
cmake-rebuild: cmake-clean cmake-build
