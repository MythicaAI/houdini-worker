ARG HOUDINI_VERSION=20.5.370

FROM aaronsmithtv/hbuild:${HOUDINI_VERSION}-base

# Setup Houdini environment
ENV SFX_CLIENT_ID=""
ENV SFX_CLIENT_SECRET=""
ENV HFS=/opt/houdini/build

# Install SideFX Labs Houdini Package
ENV SIDE_FX_LABS_VERSION=20.5.370

RUN mkdir /root/sideFXLabs/
RUN wget https://github.com/sideeffects/SideFXLabs/archive/refs/tags/${SIDE_FX_LABS_VERSION}.tar.gz -O /tmp/sideFXLabs.tar.gz
RUN tar -xzf /tmp/sideFXLabs.tar.gz -C /root/sideFXLabs/ --strip-components=1
RUN rm /tmp/sideFXLabs.tar.gz

RUN mkdir -p /root/houdini20.5/packages/
RUN cp /root/sideFXLabs/SideFXLabs.json /root/houdini20.5/packages/SideFXLabs.json
RUN sed -i 's|"$HOUDINI_PACKAGE_PATH/SideFXLabs20.5"|"/root/sideFXLabs"|g' /root/houdini20.5/packages/SideFXLabs.json

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
