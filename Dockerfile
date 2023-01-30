FROM ubuntu:20.04
LABEL org.opencontainers.image.authors="Hwijoon Lim <hwijoon.lim@kaist.ac.kr>"

ENV DEBIAN_FRONTEND=noninteractive 
RUN apt-get update -qq && apt-get -y install -qq --no-install-recommends build-essential wget tar libx11-dev autoconf xorg-dev git python3 python3-pip python-is-python3
RUN pip install -q numpy pandas matplotlib

# Download the ns-allinone-2.35.tar.gz and extract it.
WORKDIR /
RUN wget --no-check-certificate -q -O /ns-allinone-2.35.tar.gz http://sourceforge.net/projects/nsnam/files/allinone/ns-allinone-2.35/ns-allinone-2.35.tar.gz/download
RUN tar -xzf /ns-allinone-2.35.tar.gz
RUN rm -f /ns-allinone-2.35.tar.gz

# Remove the original ns-2.35 directory, and copy this repository
WORKDIR /ns-allinone-2.35
RUN rm -rf ns-2.35
COPY . ns-2.35
RUN ./install > /dev/null

WORKDIR /ns-allinone-2.35/ns-2.35
CMD ["/bin/bash"]
