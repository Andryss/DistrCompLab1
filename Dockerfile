FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends clang make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY Makefile ./
COPY src/ ./src/
COPY docs/PAs_master/pa2345_starter_code.tar.gz ./docs/PAs_master/

ARG APP_NAME=pa6
RUN make APP_NAME="$APP_NAME" \
    && cp "build/$APP_NAME" ./program \
    && cp build/runtime/pa2345_starter_code/lib64/libruntime.so ./libruntime.so

ENV LD_LIBRARY_PATH=/app
ENTRYPOINT ["/app/program"]
CMD ["-p", "5", "--mutexl"]
