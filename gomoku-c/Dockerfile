# Stage 1: The Builder
# Use the specified Ubuntu LTS (22.04) as the base image.
FROM ubuntu:22.04 AS builder

# Install all necessary build dependencies. `build-essential` includes gcc and make.
RUN apt-get update && apt-get install -y build-essential cmake git

# Set the working directory for the build process.
WORKDIR /app/source

# Copy the entire project source code into the container.
COPY . .

# from the host machine. This ensures a clean build from source inside the container.
RUN make clean

# Build all targets (gomoku, gomoku-httpd, etc.).
# This command also handles building vendored json-c from source.
RUN make all -j 4

# Run the test suite to validate the build. If tests fail, the Docker build will stop here.
RUN make test

# Expose the ports that the application will listen on.
# 8787: HTTP API port
# 8788: HAProxy agent-check port
EXPOSE 8787 8788

# Set the final command to run when the container starts.
# This starts the Gomoku HTTP daemon with:
# - HTTP API on port 8787
# - HAProxy agent-check on port 8788 (reports ready/drain status)
CMD ["./bin/gomoku-httpd", "-b", "0.0.0.0:8787", "-a", "8788", "-r", "-L", "debug"]

