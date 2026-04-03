FROM docker.io/zephyrproject/ci:v0.26.13

ARG ZEPHYR_SDK_VERSION=0.16.8
ARG ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-${ZEPHYR_SDK_VERSION}

# Install Zephyr SDK (ARM toolchain only, sufficient for nRF52840/nRF52833)
RUN wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz && \
    tar xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz -C /opt/toolchains/ && \
    rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz && \
    /opt/toolchains/zephyr-sdk-${ZEPHYR_SDK_VERSION}/setup.sh -t arm-zephyr-eabi -c

# Set environment variables
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
ENV ZEPHYR_SDK_INSTALL_DIR=${ZEPHYR_SDK_INSTALL_DIR}

# Create non-root user
ARG USERNAME=user
ARG USER_UID=1000
ARG USER_GID=${USER_UID}
RUN groupadd --gid ${USER_GID} ${USERNAME} || true && \
    useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} || true && \
    apt-get update && apt-get install -y sudo && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
    rm -rf /var/lib/apt/lists/*

USER ${USERNAME}
WORKDIR /workspace
