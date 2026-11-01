CC ?= cc
GCOV ?= gcov
PLATFORM := $(shell uname -s)-$(shell uname -m)
BUILD := build/$(PLATFORM)
COVER := coverage/$(PLATFORM)
SOURCES := test_kiosk.c ../firmware/Devices/imu.c ../firmware/App/kiosk.c ../firmware/App/line.c
FLAGS := -std=c11 -O0 -g -Wall -Wextra -Werror
.PHONY: test coverage sanitize clean
test: $(BUILD)/test_lab3 $(BUILD)/test_kiosk
	./$(BUILD)/test_kiosk
	./$(BUILD)/test_lab3
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
	mkdir -p $(COVER)/lab3
	$(CC) $(FLAGS) --coverage $(LAB3_SOURCES) -o $(COVER)/lab3/test_lab3
	./$(COVER)/lab3/test_lab3
	$(GCOV) -b -c $(COVER)/lab3/test_lab3-audit.gcno $(COVER)/lab3/test_lab3-cadence.gcno
	mv audit.c.gcov cadence.c.gcov $(COVER)/lab3/
	python3 ../tools/coverage_summary.py $(COVER)/lab3 audit.c.gcov cadence.c.gcov
sanitize:
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SOURCES) -o $(BUILD)/test_sanitize
	./$(BUILD)/test_sanitize
	$(CC) $(FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(LAB3_SOURCES) -o $(BUILD)/lab3_sanitize
	./$(BUILD)/lab3_sanitize
clean:
	rm -rf build coverage

LAB3_SOURCES := test_lab3.c ../firmware/App/audit.c ../firmware/App/cadence.c
$(BUILD)/test_lab3: $(LAB3_SOURCES) ../firmware/App/audit.h ../firmware/App/cadence.h
	mkdir -p $(BUILD)
	$(CC) $(FLAGS) $(LAB3_SOURCES) -o $@
