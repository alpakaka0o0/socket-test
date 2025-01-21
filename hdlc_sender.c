#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>

#define PORT 8080

struct Frame
{
    int opening_flag; //0x7E;
    char address; // 'B';
    unsigned char control; // 0x00;
    char data[1024]; // NULL;
    int closing_flag; // 0x7E;
};

int sock = 0, valread;
struct sockaddr_in serv_addr;
char buffer[1024] = {0};
char *ack = "ACK";
int seq_num = 0;
int re_time = 5; // renewal 타임 변수
clock_t past; // clock을 이용해 과거 시간을 저장할 변수

int menu_num = 0; //메뉴 선택용 변수
bool sabm_check = false; 

struct Frame makeFrame(int open_flag, char addr, unsigned char ctl, char* msg, int close_flag){
    struct Frame frame = {0};
    frame.opening_flag = open_flag;
    frame.closing_flag = close_flag;
    strcpy(frame.data, msg);
    frame.control = ctl;
    frame.address = addr;
    return frame;
}

int send_u_frame(int open_flag, char addr, unsigned char ctl, char* msg, int close_flag){
    struct Frame u_frame = makeFrame(open_flag,addr, ctl, msg, close_flag);
    if (send(sock, (struct Frame *) &u_frame, sizeof(u_frame), 0) < 0) {
        perror("uframe send err");
        return 0;
    }

    //ua 용 u-frame
    struct Frame ua = {0};
    valread = read(sock, &ua, sizeof(ua));
    
    // ua가 제대로 왔는지 확인
    if(ua.opening_flag == 0x7E && ua.closing_flag == 0x7E
        && ua.address == 'A' && (ua.control == 0b11001110)){
            printf("u_frame에 대한 ua 도착 : %s\n", ua.data);
            if(ctl == 0xF4){
                sabm_check = true;
                printf("연결 완료!\n"); 
            }else{
                sabm_check = false;
                printf("연결 해제 완료!\n"); 
            }
            return 1;
        }
    else{
        printf("연결 실패..\n");
        if(ua.opening_flag != 0x7E && ua.closing_flag != 0x7E){
            printf("flag 오류\n");
        }
        if(ua.address == 'A'){
            printf("addr 오류\n");
        }
        if(ua.opening_flag != 0x7E && ua.closing_flag != 0x7E){
            printf("flag 오류\n");
        }
        return 0;
    }
}

int main(int argc, char const *argv[]) {
    //연결
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported \n");
        return -1;
    }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed \n");
        return -1;
    }

    while (true)
    {
        printf("------------------\n");
        printf("choose the number \n");
        printf("1. connect \n");
        printf("2. chat\n");
        printf("3. disconnect\n");
        printf("------------------\nchoose num : ");
        scanf("%d", &menu_num);

        if(menu_num == 1){ //connect
            
            if(sabm_check == true){
                printf("이미 연결되어있습니다.\n");
                continue;
            }

            // sabm 보내고 ua 받고 확인하는 함수 호출
            send_u_frame(0x7E,'B', 0xF4, "It is sabm", 0x7E);
        }else if (menu_num == 2)
        {
            // 연결 수립 확인
            if(!sabm_check){
                printf("연결된 곳이 없습니다!\n");
                continue;
            }

            unsigned char count_send, count_ack = 0x00; // 보낸 frame과 ack 대조용
            //chat 기능 넣기
            while (1) {
                //꼭 frame에 넣어서 보내기
                //넣을 메세지 받는 용
                char *message;
                message =(char *)malloc(300);
                printf("write message : ");
                scanf("%s", message);
                
                // close the chat
                if(!strcmp(message,"exit") || !strcmp(message, "quit")){
                    free(message);
                    printf("chat 기능을 종료합니다.\n");
                    break;
                }

                unsigned send_control = count_send << 4; //0b0(send수)0000
                send_control = send_control | count_ack;
                printf("count_send : %x\n", count_send);
                printf("count_ark : %x\n", count_ack);
                printf("send_control : %x\n", send_control);

                // //i-frame 에 넣기
                struct Frame send_frame = makeFrame(0x7E, 'B', send_control, message, 0x7E); 

                // ack 올때 까지 계속 보내기
                while(1){
                    send(sock, (struct Frame *)&send_frame, sizeof(send_frame), 0);
                    //보낸 후 타이머 세기
                    
                    time_t start, end;
                    double result;
                    start = time(NULL);
                    // Wait for ACK from server
                    struct Frame responce_frame ={0}; // i frame 받기 위함
                    valread = read(sock,(struct Frame *)&responce_frame, sizeof(responce_frame));
                    // Timer end
                    end = time(NULL);
                    result = (double)(end - start);

                    if(responce_frame.control & 0b00001000 != 0b00001000){
                        printf("[ERROR] 비형식적인 frame! : flag와 addr 형식에 맞지 않음!\n");
                        exit(0);
                    }
                    unsigned char frame_rec_num = (responce_frame.control & 0b01110000) >> 4;
                    unsigned char frame_rec_ack = (responce_frame.control & 0b00001111);

                    if (count_ack < frame_rec_num){
                        printf("앞선 frame 유실! 다시 메세지 보내기 : %s\n", message); 
                        continue;
                    }
                    
                    // control의 p/f 확인
                    if((responce_frame.control & 0b00001000) == 0x00){
                        printf("%d\n", responce_frame.control & 0b00001000);
                        printf("[ERROR] 비형식적인 frame! : p/f 형식에 맞지 않음!\n");
                        exit(0);
                    }
                    
                    //만약 rec 에서 보낸 rec_num 이 작으면 다시 보내서 받을 때까지 보내
                    // count_send 보다 온 ack수가 작으면 다시보내 && count_send > frame_rec_ack
                    if((strcmp(responce_frame.data, "ack") == 0 && result <= re_time)){
                        printf("메세지 %s 에 대한 ack 도착!\n", message);
                        printf("해당 메세지에 대한 control : %x\n",
                        responce_frame.control);
                        count_ack = (int)frame_rec_num + 0x01;
                        count_ack = count_ack & 0x07;  // 8을 넘어가면 안돼...
                        count_send = frame_rec_ack &0x07;
                        free(message);
                        break;
                    }else{
                        printf("메세지 %s 에 대한 ack 오지 않음..\n", message);
                    }
                }
                printf("------------------\n");
            }
        }else if (menu_num == 3){ //disconnect
            //연결 수립 확인
            if(!sabm_check){
                printf("연결된 곳이 없습니다!\n");
                continue;
            }

            // disc 보내고 ua 받고 확인하는 함수 호출
            send_u_frame(0x7E, 'B', 0xC2, "It is disc", 0x7E); 
        }else{
            printf("지정되지 않은 번호입니다.\n");
        }
        printf("------------------\n");
    }
    
    // Close socket
    close(sock);

    return 0;
}