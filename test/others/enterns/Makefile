CFLAGS += -Wall -g
enterns: enterns.c
checkpointee: checkpointee.c
clean: rm -f enterns
build-tests: enterns checkpointee
run: build-tests
	sudo ./run_test.sh && \
	true
