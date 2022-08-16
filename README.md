- inc
  - mainly in global.h
- src
    - server.c: no change.
    - client.c: no change.
    - kernel.c: 2 minor changes
    - tju_packet.c: 1 change
      - +int jugde_flag(char *msg, uint8_t flag)
    - tju_tcp.c: 
      - -+tju_tcp_t* tju_socket()
      - -+int tju_bind(tju_tcp_t* sock, tju_sock_addr bind_addr)
      - -+int tju_listen(tju_tcp_t* sock)
      - -+tju_tcp_t* tju_accept(tju_tcp_t* listen_sock)
      - -+int tju_connect(tju_tcp_t* sock, tju_sock_addr target_addr)
      - -+int tju_send(tju_tcp_t* sock, const void *buffer, int len)
      - -+int tju_recv(tju_tcp_t* sock, void *buffer, int len)
      - -+int tju_handle_packet(tju_tcp_t* sock, char* pkt)
      - -+int tju_close(tju_tcp_t *sock)
      - +void handle_data(tju_tcp_t *sock, char *pkt)
      - +void *utils_send_thread(void *arg)
      - +void handle_signal(int sig)
      - +void add_timer(tju_tcp_t *current_sock)
      - +void handle_timer_retransmit(char *packet)
      - +void handle_timer_2MSL(tju_tcp_t *sock)
      - +void handle_timer_constant(char *packet)
      - +void cc_sendToLayer3(char *packet_buf, int packet_len, int cc_state, int cwnd, int dupACKcount, int transed_len, int LOSS)



