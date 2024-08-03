
## Chord-DHT

Chord-DHT is a project that implements a [Distributed Hash Table (DHT)](https://en.wikipedia.org/wiki/Distributed_hash_table) based on the [Chord protocol](https://en.wikipedia.org/wiki/Chord_(peer-to-peer)). This project focuses on creating a dynamic DHT that supports efficient lookups using a finger table.

### Table of Contents

- [Project Description](#project-description)
- [Installation](#installation)
- [Usage](#usage)
- [Dynamic DHT Implementation](#dynamic-dht-implementation)
- [Project Structure](#project-structure)
- [Contributing](#contributing)
- [License](#license)

### Project Description

The goal of this project is to implement a Distributed Hash Table (DHT) using the Chord protocol. A DHT distributes the storage of key-value pairs across multiple nodes, providing a decentralized approach to data management. The project involves creating a dynamic DHT that allows nodes to join and leave the network.

### Installation

To set up Chord-DHT on your local machine, follow these steps:

1. Clone the repository:
    ```bash
    git clone https://github.com/antonhtmnn/Chord-DHT.git
    ```
2. Navigate to the project directory:
    ```bash
    cd Chord-DHT
    ```
3. Compile the code:
    ```bash
    cmake -B build -DCMAKE_BUILD_TYPE=Debug
    make -C build
    ```

### Usage

#### Running the Dynamic DHT

1. Navigate to the project directory:
    ```bash
    cd Chord-DHT
    ```
2. Start the nodes as specified in the `launch.sh` script ([tmux](https://github.com/tmux/tmux) requiered):
    ```bash
    ./launch.sh
    ```
3. Use the client to interact with the DHT e.g. with:
    ```bash
    ./client localhost 4711 SET /path/to/file < file
    ./client localhost 4711 GET /path/to/file > output_file
    ./client localhost 4711 DELETE /path/to/file
    ```

### Dynamic DHT Implementation

The dynamic DHT implementation allows nodes to join and leave the network dynamically. This involves:

1. **Node Join Process:**
   - A new node finds its successor in the existing network.
   - The new node informs its successor of its arrival.
   - The predecessor of the successor adjusts its successor to the new node.
   - The new node sets its predecessor based on the successor's previous predecessor.

2. **Finger Table:**
   - Nodes maintain a finger table to optimize lookups by skipping intermediate nodes.
   - The finger table is built using periodic stabilize messages that update the network structure.

### Project Structure

The project is structured as follows:

```
Chord-DHT/
├── src/
│   ├── peer.c
│   ├── neighbor.c
│   ├── hash_table.c
│   └── ...
├── include/
│   ├── peer.h
│   ├── neighbor.h
│   ├── hash_table.h
│   └── ...
├── CMakeLists.txt
├── launch.sh
├── LICENSE
└── README.md
```

### Contributing

Contributions are welcome! Please fork the repository and create a pull request with your changes. Ensure you follow the coding conventions and include appropriate tests.

### License

This project is licensed under the MIT License.
