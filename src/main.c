#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>

#define LW_BRIDGE_SPAN 0x00005000
#define LW_BRIDGE_BASE 0xFF200000

#define FPGA_ONCHIP_SPAN 0x0003ffff         // pixel buffer는 on-chip memory영역에 mapping된다. 

#define PIXEL_DMA_BASE 0x00000020           // pixel buffer controller의 시작 주소

#define ACC_BASE 0x00000040                 // Accelerator base address

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define i_w 256                             // input feature spec
#define i_h 256

#define CLIP_8(a) (a>127?127:(a<-128?-128:a))   // clipping 연산, 8bit끼리의 곱셈 연산후에 다시 8bit로 clipping 하는 용도.
volatile int* pixel_ctrl_ptr;
volatile int pixel_buffer_start;
volatile int pixel_buf_virtual;

volatile int* conv_acc_base;

char image[i_w * i_h];
char output_image[i_w * i_h];           // blurring 결과 이미지
char padded_image[(i_w+2)*(i_h+2)];     // zero padding 결과

char mask[9] = {1,2,1,2,4,2,1,2,1};     // gaussian filter mask

void plot_pixel(int x, int y, char line_color){                     // 8bit grayscale pixel을 화면에 display.
        *(char*)(pixel_buf_virtual + (y<<9) + (x)) = line_color;
}

void zero_padding(void){ // zero padding  (256*256)  (258*258)  convolution연산 후에도 이미지 size가 변하지 않도록 함.

    int i;int j;
    for(i=0;i<i_h * i_w;i++) {
        padded_image[i] = 0;
    }

    for(i=0;i<i_h;i++){
        for(j=0;j<i_w;j++){
            padded_image[(j+1) + i_w*(i+1)] = image[j + i_w*i];
        }
    }
}

void convolution(void){             // 전체 이미지 한 장에 대한 convolution 연산
    int i;int j;int k;int m;
        short tmp;
    for(i=0;i<i_h;i++){
        for(j=0;j<i_w;j++){

            conv_1pix(j + i_w*i);
                       
        }
    }
}

void conv_1pix(int start_pix){      // 결과 이미지 1 pixel에 대한 conv 연산
    int k; int m; short tmp;
    for(k=0;k<3;k++){
        for(m=0;m<3;m++){
           tmp += (mask[3 * k + m] * \
		   	padded_image[3 * k + m  + start_pix]);                  // mask와 image 간의 element wise 곱셈 후 전체 합
        }
    }

    output_image[start_pix] = CLIP_8(tmp>>4);     // kernel의 값의 합으로 pixel intensity를 나눔 
					//  gaussian filter는 모든 값이 1 이하 이기 때문
                                                		// 이후 8bit로 truncate ==> grayscale system에 display될 수 있도록 함
    tmp=0;
}

//////component 접근 함수
void img_load(int start_pix){

        int i;int j;
        for(i=0;i<3;i++){
                for(j=0;j<3;j++){
                        *(conv_acc_base + 3*i + j)\
                        = (unsigned int)padded_image[start_pix + (i_w+2)*i + j];      // component에 32bit pixel value를 load.
                }
        }
}
char fast_conv(void){
        int tmp; char tmp_s;
        tmp =  *(conv_acc_base + 9);			// component의 연산 결과를 read.
        tmp_s = CLIP_8((tmp&0xffff));			// CLIP 연산 후 pixel값을 반환.
        return tmp_s;
}


int main(void){
        int fd;
        void* lw_virtual;

        fd = open("/dev/mem", (O_RDWR | O_SYNC));
        lw_virtual = mmap(NULL, LW_BRIDGE_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, LW_BRIDGE_BASE);

        pixel_ctrl_ptr = (volatile int*)(lw_virtual + PIXEL_DMA_BASE);         // pixel control buffer base를 virtual address로 mapping
        pixel_buffer_start = *pixel_ctrl_ptr;                                  // pixel control buffer의 값을 읽는다(실제 buffer의 address)
                                                                               // 기본적으로 onchip memory에 할당되어 있음
        printf("%x", pixel_buffer_start);

        pixel_buf_virtual = mmap(NULL, FPGA_ONCHIP_SPAN, (PROT_READ | PROT_WRITE), \    
        MAP_SHARED, fd, pixel_buffer_start); 			// pixel buffer의 주소를 virtual address로 mapping

                                        
        FILE* fp;                                                   // blurring할 image open.
        fp = fopen("lena_256.bmp", "rb");if(fp==NULL){
                printf("cannot open file.\n");
                exit(1);
        }
        fread(image, sizeof(char), 256*256, fp);

        int i;int j;

        for(i =100;i<220;i++){
                for(j =100;j<220;j++){
                        plot_pixel(j-100, i-100, image[256*(256-i) + j]);
               }
        }

        zero_padding();         // zero padding 256*256  258*258

        convolution();          // convolution (3*3) * (258*258)  (256*256)

        for(i =100;i<220;i++){
                for(j=100;j<220;j++){
                        plot_pixel(j+28, i-100, output_image[256*(256-i) + j]);
                }
        }

        ///////Software와 동일한 연산 수행
        conv_acc_base = (volatile int*)(lw_virtual + ACC_BASE);		// component base addr을 virtual address mapping
        
        for(i=0;i<i_h;i++){
                for(j=0;j<i_w;j++){
                    img_load(j+i_w*i);			// img load
                    output_image[j+i_w*i] = fast_conv();	// 연산 결과를 read 후 저장.
                }
        }
        /////////HW 기반 결과 display
        for(i=100;i<220;i++){
                for(j=100;j<220;j++){
                        plot_pixel(j-50, i, output_image[256*(256-i) + j]);	// 연산 결과 display
                }
        }


/*
        //////////////모든 알고리즘 구성요소 수행결과 비교(시간측정)
gettimeofday(&start1, NULL);

        zero_padding();		// zero padding

        gettimeofday(&end1, NULL);
        elapsed1 = end1.tv_usec - start1.tv_usec;
        printf("\npadding exe time: %ld\n", elapsed1);

        gettimeofday(&start2, NULL);

        conv_1pix(0);                               // 1 pixel에 대한 S/W conv 수행.

        gettimeofday(&end2, NULL);
        elapsed2 = end2.tv_usec - start2.tv_usec;
        printf("\nconv exe time: %ld\n", elapsed2);

        gettimeofday(&start3, NULL);

        fast_conv();				// 1 pixel에 대한 H/W conv 수행(연산 결과 read)

        gettimeofday(&end3, NULL);
        elapsed3 = end3.tv_usec - start3.tv_usec;
        printf("\nfast conv exe time: %ld\n", elapsed3);

*/


        //clear_screen();
        munmap(lw_virtual, LW_BRIDGE_BASE);
        munmap(pixel_buf_virtual, pixel_buffer_start);
        close(fd);

        return 0;
}

