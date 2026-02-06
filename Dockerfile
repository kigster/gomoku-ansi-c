# Stage 1: The Builder
# Use the specified Ubuntu LTS (22.04) as the base image.
FROM ubuntu:22.04 AS builder

# Install all necessary build dependencies. `build-essential` includes gcc and make.
RUN apt-get update && apt-get install -y build-essential cmake git

# Set the working directory for the build process.
WORKDIR /app/source

# Copy the entire project source code into the container.
COPY . .

# Clean the project to remove any pre-compiled object files or binaries
# from the host machine. This ensures a clean build from source inside the container.
RUN make clean

# Build all targets (gomoku, gomoku-httpd, etc.).
# This command also handles building vendored json-c from source.
RUN make all -j 4

# Run the test suite to validate the build. If tests fail, the Docker build will stop here.
RUN make test
# Setting PREFIX=/app causes the Makefile to use /app/bin as the installation directory for binaries.
RUN make install PREFIX=/app

# ---

# Stage 2: The Final Production Image
# Use the same Ubuntu LTS base for consistency and runtime compatibility.
FROM ubuntu:22.04

# Install only the runtime dependency needed by the `gomoku-httpd` binary.
# Clean up the apt cache afterward to keep the final image small.
RUN apt-get update && apt-get install -y libjson-c5 && rm -rf /var/lib/apt/lists/*

# Create the directory that will hold the application.
WORKDIR /app/bin

# Copy ONLY the installed binaries from the `/app/bin` directory in the builder stage.
COPY --from=builder /app/bin/ .

# Add the application's binary directory to the system's PATH.
ENV PATH="/app/bin:${PATH}"

RUN ls -al /app/bin

# Expose the ports that the application will listen on.
# 8787: HTTP API port
# 8788: HAProxy agent-check port
EXPOSE 8787 8788

# Set the final command to run when the container starts.
# This starts the Gomoku HTTP daemon with:
# - HTTP API on port 8787
# - HAProxy agent-check on port 8788 (reports ready/drain status)
CMD ["./gomoku-httpd", "-b", "0.0.0.0:8787", "-a", "8788", "-L", "debug"]

