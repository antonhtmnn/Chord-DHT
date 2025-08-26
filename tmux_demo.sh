#!/usr/bin/env bash
set -euo pipefail

SESSION="Chord-DHT-Demo"
IP="127.0.0.1"

# IDs/ports
ID1=1; PORT1=5001
ID2=2; PORT2=5002
ID3=3; PORT3=5003
ID4=4; PORT4=5004
ID5=5; PORT5=5005

# repo & binaries
REPO_DIR="${REPO_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
cd "$REPO_DIR"

command -v tmux >/dev/null || { echo "tmux not found"; exit 1; }

PEER=""
for p in "./build/peer" "./peer"; do [[ -x "$p" ]] && PEER="$p" && break; done
CLIENT=""
for c in "./build/client" "./client"; do [[ -x "$c" ]] && CLIENT="$c" && break; done
[[ -n "$PEER" && -n "$CLIENT" ]] || { echo "Missing binaries: peer/client not found."; exit 1; }

# tmux helpers
type_line()  { tmux send-keys -t "$1" -l "$2"; }
press_enter(){ tmux send-keys -t "$1" C-m; }

# fresh session
tmux has-session -t "$SESSION" 2>/dev/null && tmux kill-session -t "$SESSION"

############################
# WINDOW 1: NODES (5 panes)
############################
tmux new-session -d -s "$SESSION" -n NODES
tmux split-window -h -t "$SESSION:0"
tmux select-pane  -t "$SESSION:0.0"; tmux split-window -v -t "$SESSION:0"
tmux select-pane  -t "$SESSION:0.1"; tmux split-window -v -t "$SESSION:0"
tmux select-pane  -t "$SESSION:0.3"; tmux split-window -v -t "$SESSION:0"
tmux select-layout -t "$SESSION:0" tiled
tmux setw -t "$SESSION:0" remain-on-exit on

P0="$SESSION:0.0"  # Node 1
P1="$SESSION:0.1"  # Node 2
P2="$SESSION:0.2"  # Node 3
P3="$SESSION:0.3"  # Node 4
P4="$SESSION:0.4"  # Node 5

tmux select-pane -t "$P0"; tmux select-pane -T "Node $ID1:$PORT1"
tmux select-pane -t "$P1"; tmux select-pane -T "Node $ID2:$PORT2"
tmux select-pane -t "$P2"; tmux select-pane -T "Node $ID3:$PORT3"
tmux select-pane -t "$P3"; tmux select-pane -T "Node $ID4:$PORT4"
tmux select-pane -t "$P4"; tmux select-pane -T "Node $ID5:$PORT5"

for pn in "$P0" "$P1" "$P2" "$P3" "$P4"; do
  tmux send-keys -t "$pn" "cd \"$REPO_DIR\"" C-m "clear" C-m
done

# stage node commands (press Enter in each pane to start)
type_line "$P0" "$PEER $IP $PORT1 $ID1"
type_line "$P1" "$PEER $IP $PORT2 $ID2 $IP $PORT1"
type_line "$P2" "$PEER $IP $PORT3 $ID3 $IP $PORT2"
type_line "$P3" "$PEER $IP $PORT4 $ID4 $IP $PORT2"
type_line "$P4" "$PEER $IP $PORT5 $ID5 $IP $PORT3"

############################
# WINDOW 2: CLIENT (1 pane)
############################
tmux new-window -t "$SESSION:1" -n CLIENT
C0="$SESSION:1.0"
tmux send-keys -t "$C0" "cd \"$REPO_DIR\"" C-m "clear" C-m

# launch an interactive runner immediately; it only shows a short message and waits for Enter
type_line "$C0" "bash -lc \$'set -e
clear
echo \"Press Enter to start interacting with the DHT as specified in the demo:\"
echo \"     0) Create local file hello.txt (in project folder)\"
echo \"     1) SET hello.txt via Node 2\"
echo \"     2) GET hello.txt via Node 5\"
echo \"     3) DELETE hello.txt via Node 3\"
echo \"     4) GET hello.txt via Node 4 (expected fail)\"
echo \"\"

read -rp \"0) [Press Enter] Create local file hello.txt (in project folder)\"
printf \"Hello DHT!\" > hello.txt
echo \"✓ Created ./hello.txt\"
echo \"\"

read -rp \"1) [Press Enter] SET hello.txt via Node 2\"
\"$CLIENT\" $IP $PORT2 SET /docs/hello.txt < hello.txt
echo \"\"

read -rp \"2) [Press Enter] GET hello.txt via Node 5\"
\"$CLIENT\" $IP $PORT5 GET /docs/hello.txt
echo \"\"
echo \"\"

read -rp \"3) [Press Enter] DELETE hello.txt via Node 3\"
\"$CLIENT\" $IP $PORT3 DELETE /docs/hello.txt
echo \"\"

read -rp \"4) [Press Enter] GET hello.txt via Node 4 (expected fail)\"
\"$CLIENT\" $IP $PORT4 GET /docs/hello.txt || echo \"\"

echo \"Client demo finished.\"'"
press_enter "$C0"

# focus nodes and attach
tmux select-window -t "$SESSION:0"
tmux select-pane   -t "$P0"
tmux attach -t "$SESSION"
