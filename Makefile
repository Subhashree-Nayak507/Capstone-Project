# Top-level build.  Targets: all (app), driver, test, asan, smoke, demo, clean
all:
	$(MAKE) -C app
driver:
	$(MAKE) -C driver
test: all
	$(MAKE) -C tests run
asan:
	$(MAKE) -C app clean
	$(MAKE) -C app CXXFLAGS="-std=c++17 -O1 -g -fsanitize=address,undefined -pthread -Wall -Wextra"
	$(MAKE) -C tests CXXFLAGS="-std=c++17 -O1 -g -fsanitize=address,undefined -pthread" run
	$(MAKE) -C app clean
tsan:
	$(MAKE) -C app clean
	$(MAKE) -C app CXXFLAGS="-std=c++17 -O1 -g -fsanitize=thread -pthread -Wall -Wextra"
	$(MAKE) -C tests CXXFLAGS="-std=c++17 -O1 -g -fsanitize=thread -pthread" run
	$(MAKE) -C app clean
smoke:
	gcc -Wall -Iinclude tools/envmon_smoke.c -o build/envmon_smoke && ./build/envmon_smoke
demo: all
	scripts/demo.sh
clean:
	$(MAKE) -C app clean
	rm -f build/envmon_tests build/envmon_smoke
.PHONY: all driver test asan tsan smoke demo clean
