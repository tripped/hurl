all: hurl

hurl: main.cpp hurl.cpp
	g++ -O0 -I../include -I/opt/local/include -L/opt/local/lib -lcurl -ltar -lz -o $@ $+

clean:
	-rm hurl
