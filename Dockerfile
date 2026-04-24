# ============================================================================
# AIGuardian — Multi-stage Docker build
#
# Stage 1: Build the C++ project with CMake
# Stage 2: Minimal runtime image with only the binary + assets
# ============================================================================

# --- Build stage ---
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    make \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p build && cd build \
    && cmake .. -DCMAKE_BUILD_TYPE=Release -DGUARDIAN_BUILD_TESTS=OFF \
    && cmake --build . -j$(nproc) --target guardian_gateway

# --- Runtime stage ---
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the gateway binary
COPY --from=builder /app/build/bin/guardian_gateway /app/guardian_gateway

# Copy runtime assets
COPY policies/ /app/policies/
COPY frontend/ /app/frontend/

# Render.com sets the PORT env var; default to 8080
ENV PORT=8080

EXPOSE ${PORT}

# Health check for Render / Docker Compose
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:${PORT}/health || exit 1

# Start the gateway — serve policy API + frontend UI
CMD ["sh", "-c", "./guardian_gateway policies/demo.json ${PORT} frontend"]
