# modified from https://gitlab.eduxiji.net/nscscc/compiler2021
FROM ubuntu:20.04

WORKDIR /build

RUN apt-get update \
    && apt-get install -y build-essential clang-10 git libantlr4-runtime-dev \
    && ln -s /usr/bin/clang-10 /usr/bin/clang \
    && ln -s /usr/bin/clang++-10 /usr/bin/clang++

COPY compile.sh .

CMD ["bash", "-c", "cd /build && ./compile.sh"]
