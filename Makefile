IMAGE ?= kioskguard-build:lab2

.PHONY: image build clean test coverage sanitize

image:
	docker build -t $(IMAGE) docker

build: image
	docker run --rm -v "$(CURDIR):/workspace" -w /workspace \
		--user "$(shell id -u):$(shell id -g)" $(IMAGE) \
		make -C firmware all

clean:
	$(MAKE) -C firmware clean

test:
	python3 tools/check_structure.py
	$(MAKE) -C tests test

coverage:
	$(MAKE) -C tests coverage

sanitize:
	$(MAKE) -C tests sanitize
