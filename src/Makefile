all: hurl test

hurl: main.cpp hurl.cpp
	g++ -O0 -I../include -I/opt/local/include -L/opt/local/lib -lcurl -ltar -o $@ $+

test: test.cpp hurl.cpp
	g++ -O0 -I../include -I/opt/local/include -L/opt/local/lib -lcurl -lmicrohttpd -ltar -o $@ $+

clean:
	-rm hurl
