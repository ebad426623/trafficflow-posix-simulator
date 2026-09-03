# Traffic Flow POSIX Simulator

A multithreaded traffic-network simulator written in C and C++. The program models cars moving concurrently through shared road connectors and uses POSIX threads, mutexes, condition variables, and monitor-style synchronization to coordinate access without busy waiting.

Each car runs in its own thread and follows a predefined route through one or more connectors. The simulator logs timestamped events as cars travel, arrive, begin passing, and finish passing.

## Road connectors

The simulation supports three connector types:

- **Narrow bridge (`N`)** — connects two opposing lanes through a single shared lane. Traffic moves in one direction at a time, cars preserve their arrival order, and the direction changes when the current side empties or the opposite side reaches its maximum wait time.
- **Ferry (`F`)** — transports groups of cars independently in either direction. A ferry departs when it reaches capacity or when the first loaded car reaches the maximum wait time.
- **Crossroad (`C`)** — connects four incoming roads. Only one incoming direction is active at a time. Waiting traffic is served according to the connector's timeout and direction-ordering rules.

Cars moving in the same direction through a narrow bridge or crossroad begin passing with a small delay (`PASS_DELAY`) between them. Connector traversal and car travel times are expressed in milliseconds.

## Synchronization model

- One POSIX thread is created for every car.
- Monitor objects protect connector state with mutexes.
- Condition variables suspend cars until they are eligible to proceed.
- Timed condition waits enforce maximum waiting periods.
- Per-lane queues preserve first-in, first-out ordering.
- The main thread waits for every car thread before exiting.
- Output is protected by a mutex so each event is printed as one complete line.

Because cars run concurrently, the precise ordering and timestamps of independent events may differ between runs.

## Project structure

```text
trafficflow-posix-simulator/
|-- inc/                 # Header files
|   |-- helper.h
|   |-- monitor.h
|   `-- WriteOutput.h
|-- inputs/              # Ten sample test cases (input0.txt-input9.txt)
|-- src/                 # C and C++ source files
|   |-- helper.c
|   |-- main.cpp
|   `-- WriteOutput.c
|-- LICENSE
|-- Makefile
`-- README.md
```

The generated `simulator` executable and the original project-description PDF are not required in the repository.

## Requirements

- Linux or another environment with POSIX thread support
- GNU Make
- A C++ compiler with `g++` compatibility
- POSIX threads (`pthread`)

On Windows, run the project inside WSL or another Linux environment. The simulator relies on POSIX APIs and is not designed to compile directly with the standard Windows toolchain.

## Build

From the repository root, run:

```bash
make
```

This compiles the files in `src/`, uses headers from `inc/`, links against `pthread`, and creates an executable named `simulator` in the repository root.

The equivalent compiler command is:

```bash
g++ src/*.cpp src/*.c -Iinc -o simulator -lpthread
```

## Run

The simulator reads its complete configuration from standard input. Run one of the included cases with input redirection:

```bash
./simulator < inputs/input0.txt
```

To run every included test case:

```bash
for input in inputs/input*.txt; do
    echo "Running $input"
    ./simulator < "$input"
done
```

The `inputs/` directory contains ten scenarios:

- `input0.txt`-`input2.txt`: narrow-bridge traffic patterns
- `input3.txt`-`input5.txt`: ferry capacity, direction, and departure patterns
- `input6.txt`-`input8.txt`: crossroad traffic patterns
- `input9.txt`: narrow-bridge traffic with different car travel times and direction switching

## Input format

Connector and car IDs are zero-based and assigned in the order in which they appear in the input.

```text
NUMBER_OF_NARROW_BRIDGES
NARROW_TRAVEL_TIME NARROW_MAX_WAIT_TIME
... repeated for every narrow bridge

NUMBER_OF_FERRIES
FERRY_TRAVEL_TIME FERRY_MAX_WAIT_TIME FERRY_CAPACITY
... repeated for every ferry

NUMBER_OF_CROSSROADS
CROSSROAD_TRAVEL_TIME CROSSROAD_MAX_WAIT_TIME
... repeated for every crossroad

NUMBER_OF_CARS
CAR_TRAVEL_TIME PATH_LENGTH
CONNECTOR FROM TO CONNECTOR FROM TO ...
... two lines repeated for every car
```

For each car:

- `CAR_TRAVEL_TIME` is the time spent traveling to each connector.
- `PATH_LENGTH` is the number of connectors in the car's route.
- `CONNECTOR` combines a type and zero-based ID, such as `N0`, `F1`, or `C2`.
- `FROM` and `TO` describe the direction through that connector.
- Narrow bridges and ferries use directions `0` and `1`.
- Crossroads use directions `0` through `3`; synchronization is determined by `FROM`.

### Example input

```text
1
100 1000
0
0
2
100 2
N0 0 1 N0 0 1
100 2
N0 1 0 N0 1 0
```

This creates one narrow bridge with a 100 ms traversal time and a 1000 ms maximum wait, no ferries or crossroads, and two cars approaching the bridge from opposite directions.

## Output

Every line reports the current thread, car, connector, elapsed timestamp, action ID, and a readable action description. For example:

```text
ThreadID: ..., CarID: 0, Object: N0, time stamp: 100, AID: 1 arrived at connector.
```

The action IDs are:

| ID | Event |
|---:|---|
| `0` | Traveling to the connector |
| `1` | Arrived at the connector |
| `2` | Started passing the connector |
| `3` | Finished passing the connector |

## License

See [LICENSE](LICENSE) for license information.
