all: test1.bin test2.bin stderr.bin feed_string.bin feed_file.bin json.bin sudo.bin curl.bin

.cpp.o:
	g++ -std=c++23 -I/usr/include/libxml2 -c -Wall -o $@ $<

libpiped_subprocess.a: piped_subprocess.o
	ar r $@ $^

%.bin: tests/%.o libpiped_subprocess.a 
	g++ -o $@ $^ -lcurl -lxml2

install:
	mkdir -p /usr/local/include	
	cp -a piped_subprocess.h /usr/local/include/
	mkdir -p /usr/local/lib
	cp -a libpiped_subprocess.a /usr/local/lib/

clean:
	rm -f *.bin *.o *.a *.tmp tests/*.o
