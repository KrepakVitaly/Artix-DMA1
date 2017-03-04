#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>
#include <xdma-ioctl.h>
#include <time.h>
//#include <wz-xdma-ioctl.h>
#include <fcntl.h>
#define TOT_BUF_LEN ((long) WZ_DMA_BUFLEN * (long) WZ_DMA_NOFBUFS)
int fu=-1;
int fc=-1;
int fm=-1;
volatile uint32_t * usr_regs = NULL;
volatile uint64_t * data_buf = NULL;
struct timespec ts;
double tstart, tcur;
volatile uint64_t dummy1 = 0;

struct wz_xdma_data_block_desc bdesc;
struct wz_xdma_data_block_confirm bconf;
int64_t tot_len = 0;
int64_t old_tot_len = 0;

int start_source()
{
	usr_regs[0x10000/4]=1;
	asm volatile ("" : : : "memory");
}

int stop_source()
{
	usr_regs[0x10000/4]=0;
	asm volatile ("" : : : "memory");
}

int main(int argc, char * argv[])
{
	int res;
	fu=open("/dev/wz-xdma0_user", O_RDWR);
	if(fu<0) {
		perror("Can't open /dev/wz-xdma0_user");
		exit(1);
	};
	fc=open("/dev/wz-xdma0_control", O_RDWR);
	if(fc<0) {
		perror("Can't open /dev/wz-xdma0_control");
		exit(1);
	};
	fm=open("/dev/wz-xdma0_c2h_0",O_RDWR );
	if(fm<0) {
		perror("Can't open /dev/wz-xdma0_c2h_0");
		exit(1);
	};
	//Allocate buffers
	res=ioctl(fm,IOCTL_XDMA_WZ_ALLOC_BUFFERS,0L);
	if(res<0) {
		perror("I can't alloc DMA buffers");
		exit(3);
	}
	//Now mmap the user registers
	usr_regs = mmap(NULL,1024*1024,PROT_READ|PROT_WRITE,MAP_SHARED,fu,0);
	if(usr_regs == MAP_FAILED) {
		perror("Can't mmap user registers");
		exit(2);		
	}
	data_buf = mmap(NULL, TOT_BUF_LEN, PROT_READ|PROT_WRITE, MAP_SHARED,fm,0);
	if(data_buf == MAP_FAILED) { 
		perror("Can't mmap data");
		exit(2);		
	}
	//Ensure, that all pages are mapped
	{
		int i;
		for(i=0;i<TOT_BUF_LEN/sizeof(uint64_t);i++)
			dummy1 += data_buf[i];
	}
	//Stop the source
	stop_source();
	//Start the data acquisition
	res=ioctl(fm,IOCTL_XDMA_WZ_START,0L);
	if(res<0) {
		perror("I can't start the data source");
		exit(3);
	}
	//Start the source
	
	clock_gettime(CLOCK_MONOTONIC,&ts);
	tstart=ts.tv_sec+1.0e-9*ts.tv_nsec;
	start_source();
	while(1) {
			res=ioctl(fm,IOCTL_XDMA_WZ_GETBUF,(long) &bdesc);
			if(res<0) {
				perror("I can't get buffer");
				printf("transmitted: %ld\n",tot_len);
				exit(4);
			}
			bconf.first_desc=bdesc.first_desc;
			bconf.last_desc=bdesc.last_desc;
			tot_len += (WZ_DMA_BUFLEN*((bdesc.last_desc - bdesc.first_desc) % WZ_DMA_NOFBUFS))+bdesc.last_len;
			res=ioctl(fm,IOCTL_XDMA_WZ_CONFIRM,(long) &bconf);
			if(res<0) {
				perror("I can't confirm buffer");
				exit(4);
			}
			if(tot_len > old_tot_len + 100000000L) {
				clock_gettime(CLOCK_MONOTONIC,&ts);
				tcur=ts.tv_sec+1.0e-9*ts.tv_nsec;
				printf("transmitted: %ld time: %g rate: %g\n",tot_len, tcur-tstart, tot_len/(tcur-tstart));
				old_tot_len = tot_len;
			}
	}
}