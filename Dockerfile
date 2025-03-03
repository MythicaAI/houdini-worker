ARG HOUDINI_VERSION=20.5.370

FROM aaronsmithtv/hbuild:${HOUDINI_VERSION}-base

# Setup Houdini environment
ENV SFX_CLIENT_ID=""
ENV SFX_CLIENT_SECRET=""
ENV HFS=/opt/houdini/build

# Install build dependencies
RUN apt-get update && apt-get install -y \
    tcsh \
    g++ \
    cmake \
    python3 \
    python3-websocket \
    mesa-common-dev \
    libglu1-mesa-dev \
    libxi-dev \
    && rm -rf /var/lib/apt/lists/*

# Set HDK environment variables
ENV PATH=${HFS}/bin:${PATH}

# Create worker build directory
RUN mkdir -p /worker
WORKDIR /worker

# Copy the source and CMake files
COPY src/ src/
COPY third_party/ third_party/
COPY CMakeLists.txt .

# Create build directory and build
RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build .

# Create directory to run the worker
RUN mkdir -p /run
WORKDIR /run

COPY run.sh .
COPY assets/ assets/

RUN chmod +x run.sh
RUN cp /worker/build/houdini_worker .

ENTRYPOINT ["/bin/bash", "run.sh"]
