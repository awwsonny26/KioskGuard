IMAGE ?= kioskguard-build:lab2

.PHONY: image version build clean test coverage sanitize

version:
	python3 tools/stamp_version.py

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware BUILD_DIR=build-poll all

clean:
	$(MAKE) -C firmware BUILD_DIR=build-poll clean

test:
	python3 tools/check_structure.py
	$(MAKE) -C tests test

coverage:
	$(MAKE) -C tests coverage

sanitize:
	$(MAKE) -C tests sanitize
