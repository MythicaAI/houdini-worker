ARG LIBS_PYTHON_IMAGE=mythica-libs-python
ARG HOUDINI_VERSION=20.5.370

# Image for monorepo library dependencies
FROM ${LIBS_PYTHON_IMAGE} AS libs-python-image

# Gather python dependencies - do the worker deps here too.
# this version of python should match the target version in Houdini
FROM python:3.11-slim AS python-dependency-downloader

LABEL maintainer="jacob@mythica.ai"
LABEL name="mythica-auto-houdini"
LABEL description="Automation for mythica gen aiservices"
LABEL version="1.0.0"
LABEL tier="auto"

# Copy monorepo dependencies
COPY --from=libs-python-image /libs/python /libs/python

WORKDIR /python-install

COPY controller/requirements.txt .

# Install updated bootstrapping packages for poetry and pip
RUN python -m pip install \
    -r requirements.txt


FROM aaronsmithtv/hbuild:${HOUDINI_VERSION}-base as houdini-worker

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

# Install build dependencies and Python runtime libraries
RUN apt-get update && apt-get install -y \
    tcsh \
    g++ \
    cmake \
    python3.11 \
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

# Create build directory and build Houdini-Worker
RUN mkdir build && cd build && \
    cmake .. && \
    cmake --build .

# Copy python dependencies
COPY --from=python-dependency-downloader /libs/python /libs/python
COPY --from=python-dependency-downloader /usr/local/lib/python3.11/site-packages /usr/local/lib/python3.11/site-packages

# Ensure that Python looks in the copied site-packages directory
ENV PYTHONPATH=/usr/local/lib/python3.11/site-packages:$PYTHONPATH

RUN mkdir -p /run

WORKDIR /run

# Copy the controller and assets
COPY controller/ controller/
COPY assets/ assets/
COPY run.sh .
RUN chmod +x run.sh

# Copy the worker binary to a location in PATH so the controller can launch it.
RUN cp /worker/build/houdini_worker .

# Expose the ports used by the worker and controller:
# 8765: Public websocket for clients.
# 9876: Privileged websocket (internal).
# 8888: Websocket server for op:package commands in the controller.
EXPOSE 8765 9876 8888

# Set the container's entrypoint to run the controller,
# which in turn will launch the Houdini-Worker.
ENTRYPOINT ["/bin/bash", "run.sh"]
