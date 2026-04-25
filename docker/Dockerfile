FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    libsdl1.2-dev \
    libsdl-image1.2-dev \
    libsdl-ttf2.0-dev \
    libsdl-net1.2-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN make

CMD ["./blobwar"]