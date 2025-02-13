# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Values: debian:<version>, ubuntu:<version>
BASE_IMAGE ?= ubuntu:20.04
PYTHON_VERSION ?= 3.9
NUMPY_VERSION ?= 1.21.2

# Nightly flag for build_arm_pip_package.sh
NIGHLTY_FLAG ?= --nightly_flag

MAKEFILE_DIR := $(realpath $(dir $(lastword $(MAKEFILE_LIST))))
WORKSPACE_DIR := $(MAKEFILE_DIR)/../../../..
TAG_IMAGE := "tflite-runtime-builder-$(subst :,-,$(BASE_IMAGE))"

DOCKER_PARAMS := --pid=host \
    --env "CI_BUILD_USER=$(shell id -u -n)" \
    --env "CI_BUILD_UID=$(shell id -u)" \
    --env "CI_BUILD_GROUP=$(shell id -g -n)" \
    --env "CI_BUILD_GID=$(shell id -g)" \
		--env "CI_BUILD_HOME=$(WORKSPACE_DIR)/bazel-ci_build-cache" \
		--volume $(WORKSPACE_DIR)/bazel-ci_build-cache:$(WORKSPACE_DIR)/bazel-ci-build-cache \
		--volume $(WORKSPACE_DIR):/workspace \
		--workdir /workspace

.PHONY: help \
        docker-image \
        docker-shell \
        docker-build \
        clean

help:
	@echo "make docker-image -- build docker image"
	@echo "make docker-shell -- run shell inside the docker image"
	@echo "make docker-build -- build wheel and deb inside the docker image"
	@echo "make clean        -- remove wheel and deb files"

docker-image:
	docker build -t $(TAG_IMAGE) \
		--build-arg IMAGE=$(BASE_IMAGE) \
		--build-arg PYTHON_VERSION=$(PYTHON_VERSION) \
		--build-arg NUMPY_VERSION=$(NUMPY_VERSION) \
		-f $(MAKEFILE_DIR)/Dockerfile.py3 $(MAKEFILE_DIR)/.

docker-shell: docker-image
	mkdir -p $(WORKSPACE_DIR)/bazel-ci_build-cache
	docker run --rm --interactive --tty \
		$(DOCKER_PARAMS) \
		$(TAG_IMAGE) /with_the_same_user /bin/bash

docker-build: docker-image
	mkdir -p $(WORKSPACE_DIR)/bazel-ci_build-cache
	docker run \
		--rm --interactive $(shell tty -s && echo --tty) \
		$(DOCKER_PARAMS) \
		$(TAG_IMAGE) \
		/with_the_same_user /bin/bash -C tensorflow_lite_support/tools/pip_package/rpi/build_arm_pip_package.sh $(NIGHLTY_FLAG)

clean:
	rm -rf $(CURDIR)/wheels
