# etcd-beast

This is a simple library for communicating with ETCD. The library is incomplete as I only implemented the functions I need (set, get, watch, delete). Feel free to contribute!

##### Q: Why new library?

A: Because there is not much libraries that do that out there. There is [one library](https://github.com/nokia/etcd-cpp-apiv3) by Nokia, which is not maintained anymore and is full of bugs and is not thread-safe. Clang-thread-sanitizer was screaming all the time while I was using it. Besides, using the library is extremely complicated to compile and use, as grpc has to be embedded into that system, which itself depends on too many things.

##### Goals of the library:
This library is thread-safe. Any design changes I do, go to tests, and are executed with clang-thread-sanitizer. The library also uses the [json gateway of etcd](https://coreos.com/etcd/docs/latest/dev-guide/api_grpc_gateway.html). While this has its limitations, it is very simple.

##### Dependencies:
- boost-beast and algorithm (header-only)
- gtest, if you wanna compile with the tests (as a submodule)
- jsoncpp

Boost is added with a custom cmake file, because beast is quite new and is not available on all operating systems in the package that comes with the package manager. Please manage the cmake file as you find necessary.

##### Installation
1. Clone with 
`git clone --recursive https://github.com/TheQuantumPhysicist/etcd-beast-client`
Make sure you clone the submodules (that is why that recursive is there)
2. Edit the CMake file to where your boost library is (I recommend that you compile it yourself)
3. create a build directory
4. run `cmake -DCMAKE_INSTALL_PREFIX=/path/to/install /path/to/the/repo/that/you/cloned` from within the build directory
5. run `make`
6. run `make install`

Please do not install the library to your system as root unless you know what you are doing. I have not tried that and I never do that in my system.

##### Contibuting
Feel free to contribute by pushing to branches. Please make sure any changes you make are thread-safe by heavily testing with clang-thread-sanitizer. This is the primary requirement of this library, besides testing anything added.
