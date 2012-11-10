all: hurl

hurl: main.cpp hurl.cpp
	g++ -O0 -I../include -lcurl -o $@ $+

clean:
	-rm hurl
