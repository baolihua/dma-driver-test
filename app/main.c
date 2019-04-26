#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h> 

#define DRIVER_NAME 		        "/dev/mvpdma" 
#define AXIDMA_IOC_MAGIC 			'A'
#define AXIDMA_IOCGETCHN			_IO(AXIDMA_IOC_MAGIC, 0)
#define AXIDMA_IOCCFGANDSTART 		_IO(AXIDMA_IOC_MAGIC, 1)
#define AXIDMA_IOCGETSTATUS 		_IO(AXIDMA_IOC_MAGIC, 2)
#define AXIDMA_IOCRELEASECHN 		_IO(AXIDMA_IOC_MAGIC, 3)
#define DMA_STATUS_UNFINISHED	0
#define DMA_STATUS_FINISHED		1

struct mvpdma_chncfg {	
	unsigned int src_addr;
	unsigned int dst_addr;
	unsigned int len;	
	unsigned char chn_num;
	unsigned char status;
	unsigned char reserve[2];
	unsigned int reserve2;
};  

int main(void)
{	
	struct mvpdma_chncfg chncfg;	
	int fd = -1;	
	int ret;		
	printf("------------------------start-----------------------\n");
	
	/* open dev */	
	fd = open(DRIVER_NAME, O_RDWR);
	if(fd < 0){		
		printf("%s, %d, open %s failed\n", __FUNCTION__, __LINE__, DRIVER_NAME);
		return -1;
	}
	
	/* get channel */	
	ret = ioctl(fd, AXIDMA_IOCGETCHN, &chncfg);	
	if(ret){		
		printf("%s, %d, ioctl: get channel failed\n", __FUNCTION__, __LINE__);
		goto error;
	}	
	printf("%s, %d, channel: %d\n", __FUNCTION__, __LINE__, chncfg.chn_num);
	
	ret = ioctl(fd, AXIDMA_IOCCFGANDSTART, &chncfg);	
	if(ret){		
		printf("%s, %d, ioctl: config and start dma failed\n", __FUNCTION__, __LINE__);	
		goto error;
	} 	
	/* wait finish */
	
	while(1){
		ret = ioctl(fd, AXIDMA_IOCGETSTATUS, &chncfg);	
		if(ret){	
			printf("%s, %d, ioctl: get status failed\n", __FUNCTION__, __LINE__);
			goto error; 
		}
		
		if (DMA_STATUS_FINISHED == chncfg.status){
			break;		
		}
		
		printf("status:%d\n", chncfg.status);
		sleep(1);
	} 
	
	/* release channel */
	ret = ioctl(fd, AXIDMA_IOCRELEASECHN, &chncfg);	
	if(ret){
		printf("%s, %d, ioctl: release channel failed\n", __FUNCTION__, __LINE__);
		goto error;
	} 	
	close(fd); 
	
	return 0;
error:	
	close(fd);
	return -1;
}

