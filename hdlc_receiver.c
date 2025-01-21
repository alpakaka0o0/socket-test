스토리지의 87% 사용됨 … 스토리지가 부족하면 파일을 만들거나 수정하거나 업로드할 수 없습니다. 1개월 동안 100GB 스토리지를 ₩2,400 ₩600에 이용하세요.
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define PORT 8080
#define WINDOW_SIZE 4

//frame struct
struct Frame
{
    int opening_flag; //0x7E;
    char address; // 'B';
    unsigned char control; // 0x00;
    char data[1024]; // NULL;
    int closing_flag; // 0x7E;
}; 

int server_fd, new_socket, valread;
int opt = 1;
struct sockaddr_in address;
int addrlen = sizeof(address);
char buffer[1024] = {0};
char *ack = "ACK";
int expectedseqnum = 0;

unsigned char count_send, count_ack = 0b0000; // 보낼 frame 수와 ack (대조용)


//frame 만드는 함수
struct Frame makeFrame(int open_flag, char addr, unsigned char ctl, char* msg, int close_flag){
    struct Frame frame = {0};
    frame.opening_flag = open_flag;
    frame.closing_flag = close_flag;
    strcpy(frame.data, msg);
    frame.control = ctl;
    frame.address = addr;
    return frame;
}

// 답신 frame 보내기
int reply_ua(char * msg, char * type){
    printf("u_frame 도착! : %s\n", msg);

    struct Frame ua = makeFrame(0x7E, 'A', 0b11001110,"It is UA", 0x7E);

    //ua 보내기
    if (send(new_socket, (struct Frame *) &ua, sizeof(ua), 0) < 0) {
        perror("ua send err");
        return 0;
    }
    return 1;
}

int main(int argc, char const *argv[]) {
    //소켓 열기
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }    
    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }    
    printf("Waiting for incoming connection...\n");    

    // Accept incoming connections
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept failed");
        exit(EXIT_FAILURE);
    }    
    printf("Connection accepted\n");
    printf("---------------\n");

    //sender 로 부터 소켓 받기
    while (1){
        struct Frame rec_frame = {0};

        // Receive message from client
        valread = read(new_socket, (struct Frame *)&rec_frame, sizeof(rec_frame));

        if((rec_frame.control >> 7) == 0){
            //너는 이제부터 i frame이라고 할 꺼야
            if(rec_frame.opening_flag != 0x7E || rec_frame.closing_flag != 0x7E || rec_frame.address != 'B'){
                printf("[ERROR] 비형식적인 frame! : flag와 addr 형식에 맞지 않음!\n");
                exit(0);
            }
            printf("flag : %x, addr : %c \n", rec_frame.opening_flag, rec_frame.address);
            printf("recieved message : %s\n", rec_frame.data);
            //ack 넘버와 받은 수를 대조
            printf("recieved send control number : %x\n", rec_frame.control);
            
            // control의 p/f 확인
            if((rec_frame.control & 0b00001000) != 0x00){
                printf("%d\n", rec_frame.control & 0b00001000);
                printf("[ERROR] 비형식적인 frame! : p/f 형식에 맞지 않음!\n");
                exit(0);
            }
            // 오는 거는 0???(send가 보낸 수)0???(아크 수)
            // 받은 컨트롤에서 대조를 해야함 대조 한 후에 한 비트를 올린다.
            unsigned char frame_rec_num = (rec_frame.control & 0b01110000) >> 4;
            unsigned char frame_rec_ack = (rec_frame.control & 0b00001111);
            printf("frame_rec_num : %d\n", frame_rec_num);
            printf("frame_rec_ack : %d\n", frame_rec_ack);
            if (count_ack < frame_rec_num){
                printf("앞선 frame 유실! ack 보내기 불가 \n");
                struct Frame reject_frame = makeFrame(0x7E, 'A', 0x00, "reject", 0x7E); 
                if (send(new_socket, (struct Frame *) &reject_frame, sizeof(reject_frame), 0) < 0) {
                    perror("iframe send err");
                    return 0;
                } 
                break;
            }
            //제대로 온 경우
            count_ack = frame_rec_num + 0x01;  // 7을 넘어가면 안돼...
            count_ack = count_ack & 0x07;
            count_send = frame_rec_ack;
            printf("count_ack : %x\n",count_ack);
            printf("count_send : %x\n", count_send);

            unsigned send_control = count_send<<4; //0b0(send수)0000
            send_control = send_control | count_ack | 0b00001000;
            // printf("send_control : %x\n", send_control);

            char message[1024] = {0};
            sscanf(buffer, "%s", message);
            //ack 보내기
            //ack 보낼려면 0???(recieve가 보낸 수)1???(아크 수)
            struct Frame responce_frame = makeFrame(0x7E, 'A', send_control, "ack", 0x7E); 
            if (send(new_socket, (struct Frame *) &responce_frame, sizeof(responce_frame), 0) < 0) {
                perror("iframe send err");
                return 0;
            }  
            printf("responce_frame send_control : %x\n", responce_frame.control);
        } else{
            if(rec_frame.opening_flag != 0x7E || rec_frame.closing_flag != 0x7E || rec_frame.address != 'B'){
                printf("[ERROR] 비형식적인 frame!\n");
                exit(0);
            }
            // 너는 이제부터 u frame이라고 할 꺼야
            if(rec_frame.control == 0xF4){
                //sabm control = 11110100 으로 설정 
                //sabm인가?
                //ua 만들어 보내는 함수
                if (reply_ua(rec_frame.data, "sabm") == 0){
                    //1이면 성공, 0이면 실패  
                    printf("연결 실패..\n");
                    continue;
                }

                printf("연결 완료!\n");
            }else if(rec_frame.control == 0xC2){
                //disc control = 11000010
                //disc인가?
                //ua 만들어 보내는 함수
                if (reply_ua(rec_frame.data, "disc") == 0){
                    //1이면 성공, 0이면 실패  
                    printf("연결 해제 실패..\n");
                    continue;
                }

                printf("연결 해제 완료!\n");
                
            }else{
                printf("허용되지 않은 frame\n");
                exit(0);
            }
        }
        printf("---------------\n");
    }


    // Close socket
    close(new_socket);
	close(server_fd);

    return 0;
}