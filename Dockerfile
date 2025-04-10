# Base image with Houdini dependencies
ARG HOUDINI_VERSION=20.5.370
FROM aaronsmithtv/hbuild:${HOUDINI_VERSION}-base AS houdini-base
# Install SideFX Labs Houdini Package
ENV SIDE_FX_LABS_VERSION=20.5.370

# Use BuildKit cache mount to avoid repetitive downloads
RUN --mount=type=cache,target=/var/cache/wget \
    mkdir -p /root/sideFXLabs/ /var/cache/wget && \
    if [ ! -f /var/cache/wget/sideFXLabs-${SIDE_FX_LABS_VERSION}.tar.gz ]; then \
      wget https://github.com/sideeffects/SideFXLabs/archive/refs/tags/${SIDE_FX_LABS_VERSION}.tar.gz \
           -O /var/cache/wget/sideFXLabs-${SIDE_FX_LABS_VERSION}.tar.gz; \
    fi && \
    tar -xzf /var/cache/wget/sideFXLabs-${SIDE_FX_LABS_VERSION}.tar.gz -C /root/sideFXLabs/ --strip-components=1 && \
    mkdir -p /root/houdini20.5/packages/ && \
    cp /root/sideFXLabs/SideFXLabs.json /root/houdini20.5/packages/SideFXLabs.json && \
    sed -i 's|"$HOUDINI_PACKAGE_PATH/SideFXLabs20.5"|"/root/sideFXLabs"|g' /root/houdini20.5/packages/SideFXLabs.json

# Install build dependencies
RUN apt-get update && apt-get install -y \
    tcsh \
    g++ \
    cmake \
    python3 \
    python3-pip \
    python3-venv \
    mesa-common-dev \
    libglu1-mesa-dev \
    libxi-dev \
    && rm -rf /var/lib/apt/lists/*

# Build stage
FROM houdini-base AS builder
# Set HDK environment variables
ENV HFS=/opt/houdini/build
ENV PATH=${HFS}/bin:${PATH}
# Create worker build directory
WORKDIR /worker
# Copy the source and build files
COPY src/ src/
COPY third_party/ third_party/
COPY CMakeLists.txt .
# Build Houdini-Worker
RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build .


# Final runtime image
FROM houdini-base
LABEL maintainer="jacob@mythica.ai"
LABEL name="mythica-auto-houdini"
LABEL description="Automation for mythica gen aiservices"
LABEL version="1.0.0"
LABEL tier="auto"

# Setup Houdini environment
ENV SFX_CLIENT_ID=""
ENV SFX_CLIENT_SECRET=""
ENV HFS=/opt/houdini/build

# Create runtime directory
WORKDIR /run

# Copy built worker binary
COPY --from=builder /worker/build/houdini_worker .

# Copy runtime files
COPY controller/ controller/
COPY assets/ assets/
COPY run.sh .

# Install python controller dependencies into local runtime
WORKDIR /run/controller
RUN python3 -m venv .venv
RUN . .venv/bin/activate && python3 -m pip install -r requirements.txt

WORKDIR /run

RUN chmod +x run.sh

# Expose the ports
EXPOSE 8765 9876 8888

# Set entrypoint
ENTRYPOINT ["/bin/bash", "run.sh"]
