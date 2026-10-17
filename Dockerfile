FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends clang make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
ARG APP_NAME=pa6
COPY ${APP_NAME}.tar.gz /tmp/submission.tar.gz
RUN tar --warning=no-unknown-keyword -xzf /tmp/submission.tar.gz --strip-components=1 \
    && rm /tmp/submission.tar.gz

ENV LD_LIBRARY_PATH=/app
