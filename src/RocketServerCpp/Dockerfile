# Build stage
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y g++ cmake make && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build && cmake --build build --config Release

# Runtime stage
FROM mcr.microsoft.com/azurelinux/distroless/minimal:3.0
WORKDIR /app
COPY --from=build /src/build/RocketServerCpp .
ENV UDP_PORT=3501
ENTRYPOINT ["/app/RocketServerCpp"]
