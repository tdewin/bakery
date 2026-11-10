

run:
	podman run --rm -it --name bakery bakery
build:
	podman build -t bakery -f Containerfile
