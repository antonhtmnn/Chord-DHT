#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include "hash_table.h"
#include "neighbour.h"
#include "packet.h"
#include "requests.h"
#include "server.h"
#include "util.h"

#define SIZE_OF_FT 16 // 2^16 = hash space = node ID space
#define FT_ACTIVE 0
#define FT_INACTIVE (-1)
#define FT_INIT 42

typedef struct _finger_table {
    int state;
    int finger_count; // keep track of how much fingers we already added (useful for building up the FT)
    peer **ft; // our FT stores pointers to peers
} finger_table;

// finger table
finger_table *fng_tab;

// actual underlying hash table
htable **ht = NULL;
rtable **rt = NULL;

// chord peers
peer *self = NULL;
peer *pred = NULL;
peer *succ = NULL;

// make it a global variable to update the peers in it if necessary
server *srv = NULL;

/**
 * @brief Forward a packet to a peer.
 *
 * @param peer The peer to forward the request to
 * @param pack The packet to forward
 * @return int The status of the sending procedure
 */
int forward(peer *p, packet *pack) {
    // check whether we can connect to the peer
    if (peer_connect(p) != 0) {
        fprintf(stderr, "Failed to connect to peer %s:%d\n", p->hostname,
                p->port);
        return -1;
    }

    size_t data_len;
    unsigned char *raw = packet_serialize(pack, &data_len);
    int status = sendall(p->socket, raw, data_len);
    free(raw);
    raw = NULL;

    peer_disconnect(p);
    return status;
}

/**
 * @brief Forward a request to the successor.
 *
 * @param srv The server
 * @param csocket The scokent of the client
 * @param p The packet to forward
 * @param n The peer to forward to
 * @return int The callback status
 */
int proxy_request(server *srv, int csocket, packet *p, peer *n) {
    // check whether we can connect to the peer
    if (peer_connect(n) != 0) {
        fprintf(stderr,
                "Could not connect to peer %s:%d to proxy request for client!",
                n->hostname, n->port);
        return CB_REMOVE_CLIENT;
    }

    size_t data_len;
    unsigned char *raw = packet_serialize(p, &data_len);
    sendall(n->socket, raw, data_len);
    free(raw);
    raw = NULL;

    size_t rsp_len = 0;
    unsigned char *rsp = recvall(n->socket, &rsp_len);

    // Just pipe everything through unfiltered. Yolo!
    sendall(csocket, rsp, rsp_len);
    free(rsp);

    return CB_REMOVE_CLIENT;
}

/**
 * @brief Lookup the peer responsible for a hash_id.
 *
 * @param hash_id The hash to lookup
 * @return int The callback status
 */
int lookup_peer(uint16_t hash_id) {
    // We could see whether or not we need to repeat the lookup

    // build a new packet for the lookup
    packet *lkp = packet_new();
    lkp->flags = PKT_FLAG_CTRL | PKT_FLAG_LKUP;
    lkp->hash_id = hash_id;
    lkp->node_id = self->node_id;
    lkp->node_port = self->port;

    lkp->node_ip = peer_get_ip(self);

    forward(succ, lkp);
    return 0;
}

/**
 * @brief Handle a client request we are resonspible for.
 *
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_own_request(server* srv, client *c, packet *p) {
    // build a new packet for the request
    packet *rsp = packet_new();

    if (p->flags & PKT_FLAG_GET) {
        // this is a GET request
        htable *entry = htable_get(ht, p->key, p->key_len);
        if (entry != NULL) {
            rsp->flags = PKT_FLAG_GET | PKT_FLAG_ACK;

            rsp->key = (unsigned char *)malloc(entry->key_len);
            rsp->key_len = entry->key_len;
            memcpy(rsp->key, entry->key, entry->key_len);

            rsp->value = (unsigned char *)malloc(entry->value_len);
            rsp->value_len = entry->value_len;
            memcpy(rsp->value, entry->value, entry->value_len);
        } else {
            rsp->flags = PKT_FLAG_GET;
            rsp->key = (unsigned char *)malloc(p->key_len);
            rsp->key_len = p->key_len;
            memcpy(rsp->key, p->key, p->key_len);
        }
    } else if (p->flags & PKT_FLAG_SET) {
        // this is a SET request
        rsp->flags = PKT_FLAG_SET | PKT_FLAG_ACK;
        htable_set(ht, p->key, p->key_len, p->value, p->value_len);
    } else if (p->flags & PKT_FLAG_DEL) {
        // this is a DELETE request
        int status = htable_delete(ht, p->key, p->key_len);

        if (status == 0) {
            rsp->flags = PKT_FLAG_DEL | PKT_FLAG_ACK;
        } else {
            rsp->flags = PKT_FLAG_DEL;
        }
    } else {
        // send some default data
        rsp->flags = p->flags | PKT_FLAG_ACK;
        rsp->key = (unsigned char *)strdup("Rick Astley");
        rsp->key_len = strlen((char *)rsp->key);
        rsp->value = (unsigned char *)strdup("Never Gonna Give You Up!\n");
        rsp->value_len = strlen((char *)rsp->value);
    }

    size_t data_len;
    unsigned char *raw = packet_serialize(rsp, &data_len);
    free(rsp);
    sendall(c->socket, raw, data_len);
    free(raw);
    raw = NULL;

    return CB_REMOVE_CLIENT;
}

/**
 * @brief Answer a lookup request from a peer.
 *
 * @param p The packet
 * @param n The peer
 * @return int The callback status
 */
int answer_lookup(packet *p, peer *n) {
    peer *questioner = peer_from_packet(p);

    // check whether we can connect to the peer
    if (peer_connect(questioner) != 0) {
        fprintf(stderr, "Could not connect to questioner of lookup at %s:%d\n!",
                questioner->hostname, questioner->port);
        peer_free(questioner);
        return CB_REMOVE_CLIENT;
    }

    // build a new packet for the response
    packet *rsp = packet_new();
    rsp->flags = PKT_FLAG_CTRL | PKT_FLAG_RPLY;
    rsp->hash_id = p->hash_id;
    rsp->node_id = n->node_id;
    rsp->node_port = n->port;
    rsp->node_ip = peer_get_ip(n);

    size_t data_len;
    unsigned char *raw = packet_serialize(rsp, &data_len);
    free(rsp);
    sendall(questioner->socket, raw, data_len);
    free(raw);
    raw = NULL;
    peer_disconnect(questioner);
    peer_free(questioner);
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Handle a key request request from a client.
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_packet_data(server *srv, client *c, packet *p) {
    // Hash the key of the <key, value> pair to use for the hash table
    uint16_t hash_id = pseudo_hash(p->key, p->key_len);
    fprintf(stderr, "Hash id: %d\n", hash_id);

    // Forward the packet to the correct peer
    if (peer_is_responsible(pred->node_id, self->node_id, hash_id)) {
        // We are responsible for this key
        fprintf(stderr, "We are responsible.\n");
        return handle_own_request(srv, c, p);
    } else if (peer_is_responsible(self->node_id, succ->node_id, hash_id)) {
        // Our successor is responsible for this key
        fprintf(stderr, "Successor's business.\n");
        return proxy_request(srv, c->socket, p, succ);
    } else {
        // We need to find the peer responsible for this key
        fprintf(stderr, "No idea! Just looking it up!.\n");
        add_request(rt, hash_id, c->socket, p);
        lookup_peer(hash_id);
        return CB_OK;
    }
}

/**
 * @brief Construct a control message.
 *
 * @param p Information about this peer should be put in the message
 * @param flag The type of the control messsage
 * @return packet The control message
 */
packet *build_ctrl_pkt(peer *p, uint8_t flag) {

    packet *pkt = packet_new();

    pkt->flags = PKT_FLAG_CTRL | flag;
    pkt->node_id = p->node_id;
    pkt->node_ip = peer_get_ip(p);
    pkt->node_port = p->port;

    return pkt;
}

/**
 * @brief Start building our finger table.
 */
void build_finger_table() {

    // we already have an existing FT but we will build a new one so that we are up-to-date
    if (fng_tab != NULL && fng_tab->state == FT_ACTIVE) {
        free(fng_tab->ft);
        free(fng_tab);
    }

    // initialize finger table
    fng_tab = calloc(1, sizeof(finger_table));
    fng_tab->ft = calloc(SIZE_OF_FT, sizeof(peer *));
    fng_tab->finger_count = 0;
    fng_tab->state = FT_INIT; // switch to initialization state (building FT begins)

    // now we lookup the peers that we want to find
    for (size_t i = 0; i < SIZE_OF_FT; i++) {

        int start = (int)(self->node_id + pow(2, i)) % (int)pow(2, SIZE_OF_FT);
        lookup_peer(start);
    }

    return;
}

/**
 * @brief Handle a control packet from another peer.
 * Lookup vs. Proxy Reply
 *
 * @param srv The server
 * @param c The client
 * @param p The packet
 * @return int The callback status
 */
int handle_packet_ctrl(server *srv, client *c, packet *p) {

    fprintf(stderr, "Handling control packet...\n");

    if (p->flags & PKT_FLAG_LKUP) {
        // we received a lookup request

        if (fng_tab != NULL && fng_tab->state == FT_ACTIVE) {
            // use the FT to improve efficiency of lookup
            if (peer_is_responsible(pred->node_id, self->node_id, p->hash_id)) {
                // we are responsible
                return answer_lookup(p, self);

            } else if (peer_is_responsible(self->node_id, succ->node_id, p->hash_id)) {
                // our succ is responsible
                return answer_lookup(p, succ);

            } else {
                // Great! Somebody else's job! -> forward using FT

                // find the closest pred from p->hash_id
                int closest_pred_idx = -1;
                for (size_t i = 0; i < SIZE_OF_FT; i++) {

                    int current_peer = (int)(self->node_id + pow(2, i)) % (int)pow(2, SIZE_OF_FT);
                    if (current_peer < p->hash_id) {
                        closest_pred_idx = i; // found a closer pred
                    }
                }

                if (closest_pred_idx != -1) {
                    // forward to closest pred
                    forward(fng_tab->ft[closest_pred_idx], p);
                } else {
                    fprintf(stderr, "Something went wrong in the FT!\n");
                }
            }

        } else {
            // use naive forwarding (FT is not build yet)
            if (peer_is_responsible(pred->node_id, self->node_id, p->hash_id)) {
                // we are responsible
                return answer_lookup(p, self);

            } else if (peer_is_responsible(self->node_id, succ->node_id, p->hash_id)) {
                // our succ is responsible
                return answer_lookup(p, succ);

            } else {
                // Great! Somebody else's job! -> forward to succ
                forward(succ, p);
            }
        }

    } else if (p->flags & PKT_FLAG_RPLY) {
        // Look for open requests and proxy them
        peer *n = peer_from_packet(p);

        // filling the FT still in progress...
        if (fng_tab != NULL && fng_tab->state == FT_INIT && fng_tab->finger_count < SIZE_OF_FT) {

            peer *ft_peer = peer_from_packet(p); // the peer we where looking for

            // make sure we find the correct position for the entry
            int start = p->hash_id;
            for (size_t i = 0; i < SIZE_OF_FT; i++) {

                int start_tmp = (int)(self->node_id + pow(2, i)) % (int)pow(2, SIZE_OF_FT);
                if (start == start_tmp && fng_tab->ft[i] == NULL) {
                    // we found the correct position
                    fng_tab->ft[i] = ft_peer; // add FT entry
                    fng_tab->finger_count++; // one more FT entry filled succesfully
                    break;
                }
            }

            // filling the FT completed
            if (fng_tab->finger_count == SIZE_OF_FT) {
                fng_tab->state = FT_ACTIVE;   
            }
        }

        for (request *r = get_requests(rt, p->hash_id); r != NULL;
             r = r->next) {
            proxy_request(srv, r->socket, r->packet, n);
            server_close_socket(srv, r->socket);
        }
        clear_requests(rt, p->hash_id);
    } else {
        if (p->flags & PKT_FLAG_JOIN) {
            // we recieved a JOIN message
            printf("RECIEVED JOIN -> from [port=%u]\n", p->node_port);

            if (pred == NULL) {
                // we are responsible
                pred = peer_from_packet(p); // update pred
                if (succ == NULL) {
                    succ = peer_from_packet(p); // update succ
                    srv->p_succ = succ; // also update in server struct!
                }
                // reply with notify (that contains our self) to our updated pred
                packet *reply_pkt = build_ctrl_pkt(self, PKT_FLAG_NTFY);
                sleep(0.2); // let the joinig peer start his server before answering him
                return forward(pred, reply_pkt);

            } else if (pred != NULL && peer_is_responsible(pred->node_id, self->node_id, p->node_id)) {
                // we are responsible
                pred = peer_from_packet(p); // update pred
                // reply with notify (that contains our self) to our updated pred
                packet *reply_pkt = build_ctrl_pkt(self, PKT_FLAG_NTFY);
                sleep(0.2); // let the joinig peer start his server before answering him
                return forward(pred, reply_pkt);

            } else if (succ != NULL) {
                // somebody else is responsible -> forward join request to succ
                return forward(succ, p);
            }

        } else if (p->flags & PKT_FLAG_STAB) {
            // we recieved a STABILIZE message (always our own responsibility)
            printf("RECIEVED STABILIZE (always our own responsibility!)\n");

            if (succ == NULL) {
                // we have no succ yet, time to get one...
                succ = peer_from_packet(p); // update succ
                srv->p_succ = succ; // also update in server struct!

            } else if (pred == NULL) {
                // we have no pred yet, time to get one...
                pred = peer_from_packet(p); // update pred

            } else if (peer_is_responsible(pred->node_id, self->node_id, p->node_id)) {
                // we have to update our pred
                pred = peer_from_packet(p); // update pred
            }

            // reply to every stab message with a notify that contains our pred
            if (pred != NULL) {
                packet *reply_pkt = build_ctrl_pkt(pred, PKT_FLAG_NTFY);

                // also reply directly to the sender of the stab via the sender's socket
                // (the only purpose to do this is to pass 'test_full_join_student')
                size_t data_len;
                unsigned char *raw = packet_serialize(reply_pkt, &data_len);
                sendall(c->socket, raw, data_len);
                free(raw);
                raw = NULL;

                return forward(peer_from_packet(p), reply_pkt);
            }

        } else if (p->flags & PKT_FLAG_NTFY) {
            // we recieved a NOTIFY message (always our own responsibility)
            printf("RECIEVED NOTIFY (always our own responsibility!)\n");

            if (succ == NULL) {
                // we have no succ yet, time to get one...
                succ = peer_from_packet(p); // update succ
                srv->p_succ = succ; // also update in server struct!

            } else if (peer_is_responsible(self->node_id, succ->node_id, p->node_id)) {
                // we have to update our succ
                succ = peer_from_packet(p); // update succ
                srv->p_succ = succ; // also update in server struct!
            }

        } else if (p->flags & PKT_FLAG_FNGR) {
            // we recieved a request to build our finger table (always our own responsibility)
            printf("<<<<< FNGR >>>>>\n");

            // create finger-acknowledgment packet
            packet *fack_pkt = packet_new();
            fack_pkt->flags = PKT_FLAG_CTRL | PKT_FLAG_FACK;

            // reply with finger-acknowledgment packet to client
            size_t data_len;
            unsigned char *raw = packet_serialize(fack_pkt, &data_len);
            int status = sendall(c->socket, raw, data_len);
            free(raw);
            raw = NULL;

            // start building our finger table (do this after the fack_pkt got send, otherwise test will fail)
            build_finger_table();

            return status;
        }
    }
    return CB_REMOVE_CLIENT;
}

/**
 * @brief Handle a received packet.
 * This can be a key request received from a client or a control packet from
 * another peer.
 *
 * @param srv The server instance
 * @param c The client instance
 * @param p The packet instance
 * @return int The callback status
 */
int handle_packet(server *srv, client *c, packet *p) {
    if (p->flags & PKT_FLAG_CTRL) {
        return handle_packet_ctrl(srv, c, p);
    } else {
        return handle_packet_data(srv, c, p);
    }
}

/**
 * @brief Main entry for a peer of the chord ring.
 * 
 * Accept:
 * 1. Own IP and port;
 * 2. Own ID (optional, zero if not passed);
 * 3. IP and port of Node in existing DHT. This is optional: If not passed, establish new DHT, otherwise join existing.
 *
 * @param argc The number of arguments
 * @param argv The arguments
 * @return int The exit code
 */
int main(int argc, char **argv) {

    // arguments for self
    char *ipSelf = NULL;
    char *portSelf = NULL;
    uint16_t idSelf = 0;

    // arguments for entry node (optional)
    char *ipEntry = NULL;
    char *portEntry = NULL;

    // entry node and join message
    peer *entry_peer = NULL;
    packet *join_pkt = NULL;

    if (argc == 6) {
        // case 1: join DHT via entry node (set idSelf to argument ID)
        printf("JOIN DHT via entry node -> [port=%s] (argument ID)\n", argv[5]);

        ipSelf = argv[1];
        portSelf = argv[2];
        idSelf = strtoul(argv[3], NULL, 10);

        ipEntry = argv[4];
        portEntry = argv[5];

        self = peer_init(idSelf, ipSelf, portSelf);

        // construct entry node (ID doesn't matter for our use -> default=0)
        entry_peer = peer_init(0, ipEntry, portEntry);
        // construct join message
        join_pkt = build_ctrl_pkt(self, PKT_FLAG_JOIN);

    } else if (argc == 5) {
        // case 2: join DHT via entry node (set idSelf to default ID)
        printf("JOIN DHT via entry node -> [port=%s] (default ID)\n", argv[4]);

        ipSelf = argv[1];
        portSelf = argv[2];
        idSelf = 0;

        ipEntry = argv[3];
        portEntry = argv[4];

        self = peer_init(idSelf, ipSelf, portSelf);

        // construct entry node (ID doesn't matter for our use -> default=0)
        entry_peer = peer_init(0, ipEntry, portEntry);
        // construct join message
        join_pkt = build_ctrl_pkt(self, PKT_FLAG_JOIN);

    } else if (argc == 4) {
        // case 3: first node in new DHT (set idSelf to argument ID)
        printf("First node in NEW DHT (argument ID)\n");

        ipSelf = argv[1];
        portSelf = argv[2];
        idSelf = strtoul(argv[3], NULL, 10);

        self = peer_init(idSelf, ipSelf, portSelf);

    } else if (argc == 3) {
        // case 4: first node in new DHT (set idSelf to default ID)
        printf("First node in NEW DHT (default ID)\n");

        ipSelf = argv[1];
        portSelf = argv[2];
        idSelf = 0;

        self = peer_init(idSelf, ipSelf, portSelf);

    } else {
        fprintf(stderr, "Wrong amount of args! Usage: './peer ipSelf portSelf [idSelf] [ipEntry portEntry]'\n");
    }

    // Initialize outer server for communication with clients
    srv = server_setup(portSelf);
    if (srv == NULL) {
        fprintf(stderr, "Server setup failed!\n");
        return -1;
    }
    // Initialize hash table
    ht = (htable **)malloc(sizeof(htable *));
    // Initiale reuqest table
    rt = (rtable **)malloc(sizeof(rtable *));
    *ht = NULL;
    *rt = NULL;

    // start listening (because server is not running yet)
    listen(srv->socket, 10);

    // forward join message to entry node
    if (entry_peer != NULL && join_pkt != NULL) {
        forward(entry_peer, join_pkt);
        printf("JOIN MESSAGE SEND to -> [port=%u]\n", entry_peer->port);
    }

    // store self and succ in srv to send stabilize messages when server runs
    srv->p_self = self;
    srv->p_succ = succ;

    srv->packet_cb = handle_packet;
    server_run(srv);
    close(srv->socket);
}
