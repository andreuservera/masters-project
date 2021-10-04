# Download base image ubuntu 18.04
FROM ubuntu:18.04

MAINTAINER Andreu Servera andreumarc12@gmail.com

#LABEL about the custom image
LABEL description="Development of a Centralized Network Controller and mechanisms for its Duplication over Time-Sensitive Networking (TSN) Ethernet Networks."

# Disable Prompt During Packages Installation
# ARG DEBIAN_FRONTEND=noninteractive

# Update Ubuntu Software repository and install dependencies.
RUN apt-get update && apt-get dist-upgrade -y && apt-get install -y \
	systemd \	
	cmake \	
	git \	
	curl \
	libpcre3 \
	libpcre3-dev \
	zlib1g-dev \
	libssl-dev \
	build-essential \
	&& rm -rf /var/lib/apt/lists/*


# Upload the files onto the image
COPY ./cmake/ /libs/cnc/cmake/
COPY ./lib/ /libs/cnc/lib/
COPY ./src/ /libs/cnc/src/
COPY ./CMakeLists.txt /libs/cnc/
COPY ./dependencies/ /libs/cnc/dependencies/

# Make necessary directories
RUN mkdir ./libs/cnc/generated-configs ./libs/cnc/get-config-output


# Install libyang
RUN	mkdir ./libs/cnc/dependencies/libyang/build && \
	cd ./libs/cnc/dependencies/libyang/build && \
	cmake .. && \
	make && \
	make install


# Install libssh
RUN	mkdir ./libs/cnc/dependencies/libssh/build && \
	cd ./libs/cnc/dependencies/libssh/build && \
	cmake .. && \
	make && \
	make install


# Install sysrepo
RUN	mkdir ./libs/cnc/dependencies/sysrepo/build && \
	cd ./libs/cnc/dependencies/sysrepo/build && \
	cmake .. && \
	make && \
	make install


# Install libnetconf2
RUN	mkdir ./libs/cnc/dependencies/libnetconf2/build && \
	cd ./libs/cnc/dependencies/libnetconf2/build && \
	cmake .. && \
	make && \
	make install


RUN ldconfig && \
	mkdir ./libs/cnc/dependencies/netopeer2/build && \
	cd ./libs/cnc/dependencies/netopeer2/build && \
	cmake .. && \
	make && \
	make install

# Install cnc
RUN	mkdir ./libs/cnc/build && \
	cd ./libs/cnc/build && \
	cmake .. && \
	cmake --build . --target all



