hurl
====

Does using libcurl make you reach for an emetic? Then hurl is for you!

hurl is a stupid-simple C++ wrapper around the most basic functionality of
libcurl's easy mode. Its main design goal is to be really easy to use, not
especially efficient. Thus most HTTP requests are just a single function call
which returns a structure containing the status code, headers, and the entire
response body. No fancy buffering! You don't always HTTP GET a 100GB file,
but when you do, you load it all into memory. You are the most interesting
HTTP client in the world.

(Actually, if you use the provided `download` function, the response body
will be streamed to a file in chunks because libcurl is smart like that. Just
the `get` functions buffer the entire body. We're not crazy!)

Name
----

"hurl" stands for the following:

* Highly Usable Request Library
* Highly Unstable Request Library
* Horrible Understanding of RFC Llamas
* HURL Users R Lovely
