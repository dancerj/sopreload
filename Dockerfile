FROM debian:sid-slim

RUN apt-get clean && apt-get update && apt-get install -yq \
    g++ \
    ninja-build \
    && apt-get clean
COPY . src/
RUN cd src/ && g++ -Wall -Werror ./configure.cc -o configure && ./configure && ninja -t clean && ninja -k 0
