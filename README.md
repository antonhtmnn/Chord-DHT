## Chord-DHT

Chord-DHT is a compact implementation of a [Distributed Hash Table (DHT)](https://en.wikipedia.org/wiki/Distributed_hash_table) following the [Chord protocol](https://dl.acm.org/doi/pdf/10.1145/383059.383071).  
It supports dynamic joins/leaves and client operations `SET`, `GET`, `DELETE`.

<p align="center">
  <a href="https://www.youtube.com/watch?v=rur--VoFk_E" title="YouTube Video">
    <img src="https://img.youtube.com/vi/rur--VoFk_E/maxresdefault.jpg" alt="Demo video thumbnail" width="640" />
  </a>
  <br/>
  <sub>▶ click image to watch demo video</sub>
</p>

### Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Build](#build)
- [Quick Start: tmux Demo](#quick-start-tmux-demo)
- [Manual Usage](#manual-usage)
- [CLI Reference](#cli-reference)
- [Architecture Notes](#architecture-notes)

---

### Features

- Dynamic **Chord ring** with node **join/leave**.
- **O(log N)** lookups using a **finger table** and periodic stabilization.
- Minimal **CLI client** supporting `SET`, `GET`, `DELETE`.
- Reproducible **tmux** demo.
- Small, readable **C** codebase with a simple **CMake** build.

---

### Requirements

- **CMake ≥ 3.16** and a C compiler (**gcc** / **clang** / **MSVC**).
- **tmux** (for the demo only).
- **Linux/macOS** recommended (tested on Linux).

---

### Build

```bash
# from repo root
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

This produces:
- `build/peer`
- `build/client`

---

### Quick Start: tmux Demo

A two-window tmux workspace spins up **5 nodes** and an interactive **CLIENT** window.

```bash
# from repo root
chmod +x ./tmux_demo.sh
./tmux_demo.sh
```

**What the demo shows**

1. **Create ring**: Node 1 starts; Nodes 2, 3 and 4 join via known peers.  
2. **Store**: `SET` a file (e.g., “Hello DHT”) via Node 2.  
3. **Scale**: Node 5 joins via Node 3.  
4. **Retrieve**: `GET` the file via Node 5.  
5. **Topology change**: Node 3 leaves.
6. **Retrieve again**: `GET` the file via Node 4 after the topology changed.  
7. **Delete & verify**: `DELETE` the file; subsequent `GET` fails as expected.

---

### Manual Usage

Start peers by hand (example IDs/ports as in the demo):

```bash
# founder
./build/peer 127.0.0.1 5001 1

# joins (via known node)
./build/peer 127.0.0.1 5002 2 127.0.0.1 5001
./build/peer 127.0.0.1 5003 3 127.0.0.1 5002
./build/peer 127.0.0.1 5004 4 127.0.0.1 5002
# later: add another node
./build/peer 127.0.0.1 5005 5 127.0.0.1 5003
```

Interact with the DHT (as in the demo):

```bash
# prepare a file
echo "Hello DHT" > hello.txt

# SET via Node 2
./build/client localhost 5002 SET /docs/hello.txt < hello.txt

# GET via Node 5
./build/client localhost 5005 GET /docs/hello.txt > out.txt

# simulate a leave: press Ctrl-C in the terminal of Node 3

# GET via Node 4
./build/client localhost 5004 GET /docs/hello.txt > out.txt

# DELETE and confirm missing
./build/client localhost 5005 DELETE /docs/hello.txt
./build/client localhost 5004 GET /docs/hello.txt
```

---

### CLI Reference

```
peer   <IP> <Port> <ID> [<KnownIP> <KnownPort>]
client <Host> <Port> <SET|GET|DELETE> <Key>   # data via stdin/stdout redirection
```

**Examples**

```bash
# Start founder and join a node
peer 127.0.0.1 5001 1
peer 127.0.0.1 5002 2 127.0.0.1 5001

# Client operations
client localhost 5002 SET /docs/hello.txt < hello.txt
client localhost 5005 GET /docs/hello.txt > out.txt
client localhost 5003 DELETE /docs/hello.txt
```

---

### Architecture Notes

- **Join protocol**: a newcomer finds its **successor** using the ring’s routing, updates predecessor/successor links, and **transfers keys** that fall into its responsibility range.  
- **Finger table**: each node maintains `O(log N)` fingers for **logarithmic routing**.  
- **Stabilization**: periodic tasks (e.g., `stabilize`, `fix_fingers`, `check_predecessor`) repair links and keep routing fresh under churn.  
- **Fault model**: simple cooperative churn (terminate with Ctrl-C) is showcased; crash-fault tolerance (timeouts, retries) can be extended.

For theory & context see [Chord (Stoica et al., 2001)](https://dl.acm.org/doi/pdf/10.1145/383059.383071).
