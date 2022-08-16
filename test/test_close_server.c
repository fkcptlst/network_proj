#include "tju_tcp.h"
#include <string.h>
#include <signal.h>

int TEST_BACKEND_UDPSOCKET_ID;
int STATE = LISTEN; 
int TEST_TYPE = 0; // 0 测试双方先后断开连接的情况 1 测试双方同时断开连接的情况


uint32_t client_first_send_seq; // 客户端第一次SYN传过来的seq 
uint32_t client_first_send_FIN_seq; // 客户端第一次FIN传过来的seq 


void fflushbeforeexit(int signo){
    exit(0);
}



void sendToLayer3Test(char* packet_buf, int packet_len){
    if (packet_len>MAX_LEN){
        printf("ERROR: 不能发送超过 MAX_LEN 长度的packet, 防止IP层进行分片\n");
        return;
    }

    // 获取hostname 根据hostname 判断是客户端还是服务端
    char hostname[8];
    gethostname(hostname, 8);

    struct sockaddr_in conn;
    conn.sin_family      = AF_INET;            
    conn.sin_port        = htons(20218);
    int rst;
    if(strcmp(hostname,"server")==0){
        conn.sin_addr.s_addr = inet_addr("10.0.0.2");
        rst = sendto(TEST_BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else if(strcmp(hostname,"client")==0){       
        conn.sin_addr.s_addr = inet_addr("10.0.0.1");
        rst = sendto(TEST_BACKEND_UDPSOCKET_ID, packet_buf, packet_len, 0, (struct sockaddr*)&conn, sizeof(conn));
    }else{
        printf("请不要改动hostname...\n");
        exit(-1);
    }
}

void onTCPPocketTest(char* pkt){
    
    // 获得包的各项数据
    uint16_t pkt_src = get_src(pkt);
    uint16_t pkt_dst = get_dst(pkt);
    uint32_t pkt_seq = get_seq(pkt);
    uint32_t pkt_ack = get_ack(pkt);
    uint32_t pkt_flags = get_flags(pkt);
    printf("[服务端] 收到一个TCP数据包 src=%d dst=%d seq=%d ack=%d flags=%d\n", pkt_src, pkt_dst, pkt_seq, pkt_ack, pkt_flags);

    // 获取 TCP包的 标志位
    int syn_flag=0, ack_flag=0, fin_flag=0;
    if ( (get_flags(pkt)>>3) & 1 == 1 ){ // SYN 是第四位
        syn_flag = 1;
    }
    if ( (get_flags(pkt)>>2) & 1 == 1 ){ // ACK 是第三位
        ack_flag = 1;
    }
    if ( (get_flags(pkt)>>1) & 1 == 1 ){ // FIN 是第二位
        fin_flag = 1;
    }


    if(STATE==LISTEN){
        printf("[服务端] 此时服务器状态为 LISTEN, 检查SYN数据包\n");
        int success=1;

        // 此时应该接受SYN包
        if (syn_flag!=1){
            printf("[服务端] 接收到的第一个TCP包SYN标志位不为 1\n");
            success = 0;
        }

        // SYN 的 dst应该是调用connect时传入的1234
        if (pkt_dst!=1234){
            printf("[服务端] 接收到的SYN的dst不是调用connect时传入的 1234\n");
            success = 0;
        }

        if (success){
            printf("[服务端] 客户端发送的第一个SYN报文检验通过\n");
            
            STATE = SYN_RECV;

            client_first_send_seq = pkt_seq;
            

            // 发送SYNACK
            char* msg;
            uint32_t seq = 646; // 随机seq
            uint16_t plen = DEFAULT_HEADER_LEN;
            uint32_t send_ack_val = pkt_seq + 1;
            msg = create_packet_buf((uint16_t)1234, pkt_src, 
                                    seq, send_ack_val, 
                                    DEFAULT_HEADER_LEN, plen, SYN_FLAG_MASK|ACK_FLAG_MASK, 32, 0, NULL, 0);
            printf("[服务端] 发送SYNACK 进入SYN_RECV状态 等待客户端第三次握手的ACK\n");
            sendToLayer3Test(msg, plen);
            free(msg);
            printf("[服务端] 发送SYNACK src=%d dst=%d seq=%d ack=%d flags=%d\n", 1324, pkt_src, seq, send_ack_val, SYN_FLAG_MASK|ACK_FLAG_MASK);
        }else{
            printf("[服务端] 客户端发送的第一个SYN数据包出现问题, 结束测试\n");
            exit(-1);
        }
        
    }else if(STATE==SYN_RECV){
        printf("[服务端] 此时服务器状态为 SYN_RECV, 检查ACK数据包\n");
        int success=1;

        // 此时应该收到ACK
        if (syn_flag==1||ack_flag!=1){
            printf("[服务端] 接收到的第三次握手的SYN应该为 0 ACK应该为 1\n");
            success = 0;
        }

        // ACK 的 dst 应该是1234
        if (pkt_dst!=1234){
            printf("[服务端] 第三次握手 接收到的ACK的dst不是调用connect时传入的 1234\n");
            success = 0;
        }

        // ACK的ack值应该是服务端发送的SYNACK的seq+1
        if (pkt_ack!=647){
            printf("[服务端] 第三次握手 ACK的ack值应该是服务端发送的SYNACK的 seq+1\n");
            success = 0;   
        }

        if (success){
            printf("[服务端] 客户端发送的ACK报文检验通过, 成功建立连接, 服务端状态转为ESTABLISHED\n");
            STATE = ESTABLISHED;
        }else{
            printf("[服务端] 客户端发送的第三次握手ACK数据包出现问题, 结束测试\n");
            exit(-1);
        }
    }else if(STATE==ESTABLISHED){
        printf("[服务端] 此时服务器状态为 ESTABLISHED/FIN_WAIT_1, 检查FIN数据包\n");
        int success=1;

        // 此时应该受到FIN
        if (fin_flag!=1){
            printf("[服务端] 连接建立后 理论上客户端没有进行数据收发直接调用了 tju_close 然而服务端收到了FIN标志位不为1的包\n");
            return;
            success = 0;   
        }

        // 记录下客户端发送的第一个FIN的seqnum
        client_first_send_FIN_seq = pkt_seq;

        if (success){
            printf("[服务端] 客户端发送的FIN报文检验通过\n");
            printf("{{FIRST FIN PASSED TEST}}\n");
            
            
            if (TEST_TYPE==0){
                
                printf("[服务端] 模拟正常双方先后断开连接情况 服务端先响应客户端的FIN 发送ACK 然后等待1s 发FIN ACK\n");
                // 发送ACK
                char* msg;
                uint32_t seq = 647;  // 这是第三次握手 客户端发来ACK的 acknum
                uint16_t plen = DEFAULT_HEADER_LEN;
                uint32_t send_ack_val = pkt_seq + 1;
                msg = create_packet_buf((uint16_t)1234, pkt_src, 
                                        seq, send_ack_val, 
                                        DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, 32, 0, NULL, 0);
                sendToLayer3Test(msg, plen);
                free(msg);
                printf("[服务端] 发送ACK src=%d dst=%d seq=%d ack=%d flags=%d\n", 1324, pkt_src, seq, send_ack_val, ACK_FLAG_MASK);
                STATE = CLOSE_WAIT;

                sleep(1);
                // 发送FIN ACK

                seq = 647; 
                plen = DEFAULT_HEADER_LEN;
                send_ack_val = pkt_seq + 1;
                msg = create_packet_buf((uint16_t)1234, pkt_src, 
                                        seq, send_ack_val, 
                                        DEFAULT_HEADER_LEN, plen, FIN_FLAG_MASK|ACK_FLAG_MASK, 32, 0, NULL, 0);
                sendToLayer3Test(msg, plen);
                free(msg);
                printf("[服务端] 发送FINACK src=%d dst=%d seq=%d ack=%d flags=%d\n", 1324, pkt_src, seq, send_ack_val, FIN_FLAG_MASK|ACK_FLAG_MASK);

                STATE = LAST_ACK;
                printf("[服务端] 状态转为 LAST_ACK\n");
            }else if (TEST_TYPE==1){
                STATE = FIN_WAIT_1;

                printf("[服务端] 模拟正常双方同时断开连接情况 服务端假装没有收到FIN包 先按照没有收到FIN的情况发FIN\n");
                printf("[服务端] 然后再收到客户端的FIN, 发送ACK响应\n");
                // 假装没有收到FIN包发送FIN ACK
                char* msg;
                uint32_t seq = 647; 
                uint16_t plen = DEFAULT_HEADER_LEN;
                uint32_t send_ack_val = client_first_send_seq + 1;
                msg = create_packet_buf((uint16_t)1234, pkt_src, 
                                        seq, send_ack_val, 
                                        DEFAULT_HEADER_LEN, plen, FIN_FLAG_MASK|ACK_FLAG_MASK, 32, 0, NULL, 0);
                sendToLayer3Test(msg, plen);
                free(msg);
                printf("[服务端] 发送FINACK src=%d dst=%d seq=%d ack=%d flags=%d\n", 1324, pkt_src, seq, send_ack_val, FIN_FLAG_MASK|ACK_FLAG_MASK);


                // 此时收到了FINACK 响应发送ACK
                seq = 648; 
                plen = DEFAULT_HEADER_LEN;
                send_ack_val = pkt_seq + 1;
                msg = create_packet_buf((uint16_t)1234, pkt_src, 
                                        seq, send_ack_val, 
                                        DEFAULT_HEADER_LEN, plen, ACK_FLAG_MASK, 32, 0, NULL, 0);
                sendToLayer3Test(msg, plen);
                free(msg);
                printf("[服务端] 发送ACK src=%d dst=%d seq=%d ack=%d flags=%d\n", 1324, pkt_src, seq, send_ack_val, ACK_FLAG_MASK);

                STATE = CLOSING;
                printf("[服务端] 状态转为 CLOSING\n");
            }
            
            
        }else{
            printf("[服务端] 客户端发送的第一个FIN数据包出现问题, 结束测试\n");
            exit(-1);
        }
    }else if(STATE==LAST_ACK || STATE==CLOSING){
        printf("[服务端] 此时服务器状态为 LAST_ACK/CLOSING, 检查ACK数据包\n");
        int success=1;

        // 此时应该受到ACK
        if (ack_flag!=1){
            printf("[服务端] 服务端进入LASTACK/CLOSING状态后 收到的数据包ack标志位不为 1\n");
            success = 0;   
        }

        // 检查ACK的 seqnum 和 acknum 
        // 这个ACK包的seqnum应该等于之前服务端发的FINACK的acknum 也等于自己最开始发的FINACK的seqnum+1
        // 这个ACK包的acknum应该等于服务端发送的FINACK的seqnum+1
        // 在这里检查最后的ACK 如果对了 那么四次挥手的每个包的seq和ack都应该是对的
        if (pkt_seq!=client_first_send_FIN_seq + 1){
            printf("[服务端] 收到的ACK包的seqnum应该等于自己最开始发的FINACK的seqnum+1(%d) 而实际是%d\n", client_first_send_FIN_seq+1, pkt_seq);
            success = 0;   
            exit(-1);   
        }
        if (pkt_ack!=648){
            printf("[服务端] 收到的ACK包的acknum应该等于服务端发送的FINACK的seqnum+1(648) 而实际是%d\n", pkt_ack);
            success = 0;   
            exit(-1);
        }

        
        if(success){
            printf("[服务端] 客户端发送的ACK报文检验通过\n");
            printf("{{FINAL ACK PASSED TEST}}\n");
            STATE = CLOSED;
            exit(0);
        }
    }

    return;
}


void* receive_thread_test(void* arg){

    char hdr[DEFAULT_HEADER_LEN];
    char* pkt;

    uint32_t plen = 0, buf_size = 0, n = 0;
    int len;

    struct sockaddr_in from_addr;
    int from_addr_size = sizeof(from_addr);


    while(1) {
        // MSG_PEEK 表示看一眼 不会把数据从缓冲区删除 
        // NO_WAIT 表示不阻塞 用于计时
        len = recvfrom(TEST_BACKEND_UDPSOCKET_ID, hdr, DEFAULT_HEADER_LEN, MSG_PEEK|MSG_DONTWAIT, (struct sockaddr *)&from_addr, &from_addr_size);
        // 一旦收到了大于header长度的数据 则接受整个TCP包
        if(len >= DEFAULT_HEADER_LEN){
            plen = get_plen(hdr); 
            pkt = malloc(plen);
            buf_size = 0;
            while(buf_size < plen){ // 直到接收到 plen 长度的数据 接受的数据全部存在pkt中
                n = recvfrom(TEST_BACKEND_UDPSOCKET_ID, pkt + buf_size, plen - buf_size, NO_FLAG, (struct sockaddr *)&from_addr, &from_addr_size);
                buf_size = buf_size + n;
            }
            // 收到一个完整的TCP报文
            onTCPPocketTest(pkt);
            free(pkt);
        }
    }
}


int main(int argc, char **argv) {
    signal(SIGHUP, fflushbeforeexit);
    signal(SIGINT, fflushbeforeexit);
    signal(SIGQUIT, fflushbeforeexit);

    if (argc==2){
        TEST_TYPE = atoi(argv[1]);
        if (TEST_TYPE==0){
            printf("[服务端] 测试双方先后断开连接的情况\n");
        }
        if (TEST_TYPE==1){
            printf("[服务端] 测试双方同时断开连接的情况\n");
        }
    }else{
        printf("[服务端] 测试双方先后断开连接的情况\n");
    }


    
    TEST_BACKEND_UDPSOCKET_ID  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (TEST_BACKEND_UDPSOCKET_ID < 0){
        printf("ERROR opening socket\n");
        exit(-1);
    }

    // 设置socket选项 SO_REUSEADDR = 1 
    // 意思是 允许绑定本地地址冲突 和 改变了系统对处于TIME_WAIT状态的socket的看待方式 
    int optval = 1;
    setsockopt(TEST_BACKEND_UDPSOCKET_ID, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

    struct sockaddr_in conn;
    memset(&conn, 0, sizeof(conn)); 
    conn.sin_family = AF_INET;
    conn.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = 0.0.0.0
    conn.sin_port = htons((unsigned short)20218);

    if (bind(TEST_BACKEND_UDPSOCKET_ID, (struct sockaddr *) &conn, sizeof(conn)) < 0){
        printf("ERROR on binding\n");
        exit(-1);
    }

    pthread_t recv_thread_id = 998;
    STATE = LISTEN;
    int rst = pthread_create(&recv_thread_id, NULL, receive_thread_test, (void*)(&TEST_BACKEND_UDPSOCKET_ID));
    if (rst<0){
        printf("ERROR open thread\n");
        exit(-1); 
    }

    pthread_join(recv_thread_id, NULL); 
    return 0;
}

