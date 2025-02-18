# Wavy

## **DEPENDENCIES**

1. FFmpeg (should come with its libraries)
2. base-devel (g++ / clang++ work as CXX COMPILERS)
3. OpenSSL
4. Boost C++ libs 
5. libzstd (the Z-Standard lossless compression algorithm library)
5. CMake and Make (GNU-Make) [Build System]

## **BUILDING**

To build just run:

```bash 
make encoder # to build encode.cpp
make decoder # to build decode.cpp
make dispatcher 
make server
make remove # to cleanup all the transport streams and playlists

make all
```

## **ARCHITECTURE**

<img src="assets/wavy-arch.jpeg" alt="Description" width="350">

Read [ARCHITECTURE](https://github.com/nots1dd/wavy/blob/main/ARCHITECTURE.md) for more on the intended working model of the project.

## **API REFERENCES**

Check out [API](https://github.com/nots1dd/wavy/blob/main/APIREF.md)

## **SERVER**

To generate certificates:

```bash 
openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
# email is not necessary
```

Or just Makefile:

```bash 
make server-cert # fill out the fields (email not necessary)
```

> [!NOTE]
> 
> This server.crt and server.key should be placed in the CWD of wavy
> 

> [!WARNING]
> 
> This is a self-signed certificate! 
> 
> We will register to a domain and get a valid certificate in the future but for testing purposes,
> 
> Ensure that when using:
> --> cURL to test the server --> append `-k` flag to your command 
> --> VLC/MPV let it know that you accept this self signed certificate and acknowledge that VLC/MPV cannot validate it.
> 

## **DOCUMENTATION**

Have Doxygen installed.

Run the following commands to view the docs in your browser:

```bash 
doxygen .Doxyfile
xdg-open docs/html/index.html # should open the docs in your browser (default)
```
