# Build stage
FROM ubuntu:24.04 AS build
RUN apt-get update && \
    apt-get install -y g++ cmake make && \
    rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build && \
    cmake --build build --config Release

# Runtime stage
# https://mcr.microsoft.com/en-us/artifact/mar/azurelinux/distroless/minimal
FROM mcr.microsoft.com/azurelinux/distroless/minimal:3.0 AS runtime
WORKDIR /app
COPY --from=build /src/build/RocketConsole .
ENTRYPOINT ["/app/RocketConsole"]
