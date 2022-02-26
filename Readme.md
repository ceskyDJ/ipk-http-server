# HTTP info server

HTTP info server is the 1st project to subject IPK (Computer Communications and Networks). It provides a script that acts as an HTTP server informing about the system where it is running.

## Compilation

Installation of this project could be done by using the Make program. The default rule from the packed Makefile does this job. You can compile the whole script with:
```
make
```

The compilation phase will produce one binary file called `hinfosvc`, a few obj files, and a dep.list. You can run Make again for removing temporarily created files: `make clean`.

## Running the script

This project has only one executable binary file - `hinfosvc`. It starts the HTTP server providing information about the machine where the script is running. To run the script, use this command structure:
```
./hinfosvc PORT &
```

For example: `./hinfosvc 1221` runs the server on port 1221. The server will be available at all IP (v4 and v6) addresses of the machine. For testing, you can use `http://localhost:1221` with the address of the wanted information (see next section).

## Usage

There are three types of information the server provides. You can find them in the following subsections.

### Hostname

The first information you can get from the server is the fully qualified hostname of the computer.

```
GET http://server-name:PORT/hostname
```

**Example request (from internet browser):**
```
http://localhost:1221/hostname
```

**Example output (`text/plain`):**

On personal computer (not in a domain):
```
xsmahe01-notebook
```

On school/enterprise machine (in a domain):
```
minerva3.fit.vutbr.cz
```

### CPU name

The second information you can get is the name of the used CPU.

```
GET http://server-name:PORT/cpu-name
```

**Example request (from internet browser):**
```
http://localhost:1221/cpu-name
```

**Example output (`text/plain`):**
```
Intel(R) Core(TM) i7-8550U CPU @ 1.80GHz
```

### CPU load

The last information provided by the HTTP server is the CPU load. It is the average usage of the CPU (across its cores).

```
GET http://server-name:PORT/load
```

**Example request (from internet browser):**
```
http://localhost:1221/load
```

**Example output (`text/plain`):**
```
8%
```
