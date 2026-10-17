CC ?= cc
GCOV ?= gcov
PLATFORM := $(shell uname -s)-$(shell uname -m)
BUILD := build/$(PLATFORM)
COVER := coverage/$(PLATFORM)
SOURCES := test_kiosk.c ../firmware/Devices/imu.c ../firmware/App/kiosk.c ../firmware/App/line.c
FLAGS := -std=c11 -O0 -g -Wall -Wextra -Werror
.PHONY: test coverage sanitize clean
test: $(BUILD)/test_kiosk
	./$(BUILD)/test_kiosk
$(BUILD)/test_kiosk: $(SOURCES) $(wildcard ../firmware/App/*.h ../firmware/Devices/*.h fixtures/*.h)
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) $(SOURCES) -o $@
coverage:
	mkdir -p $(COVER)
	rm -f $(COVER)/*.gcda $(COVER)/*.gcno $(COVER)/*.gcov
	$(CC) $(FLAGS) --coverage $(SOURCES) -o $(COVER)/test_kiosk
	./$(COVER)/test_kiosk
	$(GCOV) -b -c $(COVER)/test_kiosk-imu.gcno $(COVER)/test_kiosk-kiosk.gcno $(COVER)/test_kiosk-line.gcno
	mv *.c.gcov $(COVER)/
	python3 ../tools/coverage_summary.py $(COVER)
sanitize:
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $(BUILD)/test_sanitize
	./$(BUILD)/test_sanitize
clean:
	rm -rf build coverage
