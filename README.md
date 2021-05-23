# Raport de analiza
[Analiza](https://docs.google.com/document/d/1U1CrCt4-I_2qrXzfre6OU5KdJAmg8am1RlijKM3MC4I)

# Documentatie
[Documentatie](https://docs.google.com/document/d/1zbsQ2z-aaY_O1OpmMubE-6lUG5OUfGLlhEdu6eDCvUg)

# Example IoT Device

## Getting Started

Git clone this project in your machine.

### Prerequisites

Build tested on Ubuntu Server. Pistache doesn't support Windows, but you can use something like [WSL](https://docs.microsoft.com/en-us/windows/wsl/install-win10) or a virtual machine with Linux.

You will need to have a C++ compiler. I used g++ that came preinstalled. Check using `g++ -v`

You will need to install the [Pistache](https://github.com/pistacheio/pistache) library.
On Ubuntu, you can install a pre-built binary as described [here](http://pistache.io/docs/#installing-pistache).

### Building

#### Using Make

You can build the `Magic Shower` executable by running `make`.

### Manually

A step by step series of examples that tell you how to get a development env running

You should open the terminal, navigate into the root folder of this repository, and run\
`g++ shower.cpp -o shower -lpistache -lcrypto -lssl -lpthread`

This will compile the project using g++, into an executable called `shower` using the libraries `pistache`, `crypto`, `ssl`, `pthread`. You only really want pistache, but the last three are dependencies of the former.
Note that in this compilation process, the order of the libraries is important.

### Running

To start the server run\
`./shower`

Your server should display the number of cores being used and no errors.

To test, open up another terminal, and type\
`curl http://localhost:9080/turnWaterOn`

Number 1 should display.

"Water has been turned on" should display.


## Built With

* [Pistache](https://github.com/pistacheio/pistache) - Web server

## License

This project is licensed under the Apache 2.0 Open Source Licence - see the [LICENSE](LICENSE) file for details
