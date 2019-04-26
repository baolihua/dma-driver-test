/* * mvp dma test
     author: 
     date: 2019.04.25
*/
  
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/dmaengine.h> 
#include <linux/slab.h>

#define DRIVER_NAME 		        "mvpdma"
#define AXIDMA_IOC_MAGIC 			'A'
#define AXIDMA_IOCGETCHN			_IO(AXIDMA_IOC_MAGIC, 0)
#define AXIDMA_IOCCFGANDSTART 		_IO(AXIDMA_IOC_MAGIC, 1)
#define AXIDMA_IOCGETSTATUS 		_IO(AXIDMA_IOC_MAGIC, 2)
#define AXIDMA_IOCRELEASECHN 		_IO(AXIDMA_IOC_MAGIC, 3) 
#define AXI_DMA_MAX_CHANS 			8
#define DMA_CHN_UNUSED 		0
#define DMA_CHN_USED 		1 

struct mvpdma_chncfg {	
	unsigned int src_addr;	
	unsigned int dst_addr;	
	unsigned int len;	
	unsigned char chn_num;	
	unsigned char status;	
	unsigned char reserve[2];	
	unsigned int reserve2;
}; 

struct mvpdma_chns {	
	struct dma_chan *dma_chan;	
	unsigned char used;
	#define DMA_STATUS_UNFINISHED	0
	#define DMA_STATUS_FINISHED		1	
	unsigned char status;	
	unsigned char reserve[2];
}; 

struct mvpdma_chns channels[AXI_DMA_MAX_CHANS]; 
static int mvpdma_open(struct inode *inode, struct file *file)
{	
	printk("Open: do nothing\n");	
	return 0;
}

static int mvpdma_release(struct inode *inode, struct file *file)
{	
	printk("Release: do nothing\n");	
	return 0;
} 

static ssize_t mvpdma_write(struct file *file, const char __user *data, size_t len, loff_t *ppos)
{	
	printk("Write: do nothing\n");	
	return 0;
} 

static void dma_complete_func(void *status)
{	
	*(char *)status = DMA_STATUS_FINISHED;	
	printk("dma_complete!\n");
} 


static long mvpdma_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{	
	struct dma_device *dma_dev;	
	struct dma_async_tx_descriptor *tx = NULL;	
	dma_cap_mask_t mask;	
	dma_cookie_t cookie;	
	enum dma_ctrl_flags flags; 
	struct mvpdma_chncfg chncfg;	
	int ret = -1;	
	int i;
	unsigned int *src_addr = NULL;
	unsigned int *dst_addr = NULL;
	
	memset(&chncfg, 0, sizeof(struct mvpdma_chncfg));	
	switch(cmd)	
	{		
		case AXIDMA_IOCGETCHN:		
		{			
			for(i=0; i<AXI_DMA_MAX_CHANS; i++) 
			{				
				if(DMA_CHN_UNUSED == channels[i].used)
					break;							
			}
			
			if(AXI_DMA_MAX_CHANS == i)
			{				
				printk("Get dma chn failed, because no idle channel\n");
				goto error;			
			}else{				
				channels[i].used = DMA_CHN_USED;
				channels[i].status = DMA_STATUS_UNFINISHED;	
				chncfg.chn_num = i;	
				chncfg.status = DMA_STATUS_UNFINISHED;	
			} 
			
			dma_cap_zero(mask);		
			dma_cap_set(DMA_MEMCPY, mask); 
			channels[i].dma_chan = dma_request_channel(mask, NULL, NULL);	
			if(!channels[i].dma_chan)
			{				
				printk("dma request channel failed\n");
				channels[i].used = DMA_CHN_UNUSED;	
				goto error;		
			} 			
			ret = copy_to_user((void __user *)arg, &chncfg, sizeof(struct mvpdma_chncfg));
			if(ret){				
				printk("Copy to user failed\n");	
				goto error;		
			}		
		}		
		break;		
		case AXIDMA_IOCCFGANDSTART:		
		{			
			ret = copy_from_user(&chncfg, (void __user *)arg, sizeof(struct mvpdma_chncfg));
			if(ret){				
				printk("Copy from user failed\n");
				goto error;			
			} 			
			if((chncfg.chn_num >= AXI_DMA_MAX_CHANS) || (!channels[chncfg.chn_num].dma_chan))
			{				
				printk("chn_num[%d] is invalid\n", chncfg.chn_num);		
				goto error;			
			} 

			src_addr = kmalloc(4096, GFP_KERNEL);
			dst_addr = kmalloc(4096, GFP_KERNEL);
			if(!src_addr || !dst_addr){
				printk("%s, %d, Failed to kmalloc mem!\n", __FUNCTION__, __LINE__);			
				goto error;	
			}

			memset(src_addr, 0, 4096);
			memset(dst_addr, 0, 4096);

			if(src_addr){
				for(i=0; i<4096; i++){
					*src_addr++ = 0x9;
				}
			}
			chncfg.src_addr = virt_to_phys(src_addr);
			chncfg.dst_addr = virt_to_phys(dst_addr);
			chncfg.len = 4096;
			
			dma_dev = channels[chncfg.chn_num].dma_chan->device;
			flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
			tx = dma_dev->device_prep_dma_memcpy(channels[chncfg.chn_num].dma_chan, chncfg.dst_addr, chncfg.src_addr, chncfg.len, flags);
			if(!tx)
			{				
				printk("Failed to prepare DMA memcpy\n");			
				goto error;	
			}			
			tx->callback = dma_complete_func;
			channels[chncfg.chn_num].status = DMA_STATUS_UNFINISHED;
			tx->callback_param = &channels[chncfg.chn_num].status;	
			cookie =  tx->tx_submit(tx);	
			if(dma_submit_error(cookie))
			{				
				printk("Failed to dma tx_submit\n");
				goto error;	
			}			
			dma_async_issue_pending(channels[chncfg.chn_num].dma_chan);	
		}		
		break;		
		case AXIDMA_IOCGETSTATUS:	
		{			
			ret = copy_from_user(&chncfg, (void __user *)arg, sizeof(struct mvpdma_chncfg));
			if(ret){				
				printk("Copy from user failed\n");	
				goto error;		
			} 			
			if(chncfg.chn_num >= AXI_DMA_MAX_CHANS){
				printk("chn_num[%d] is invalid\n", chncfg.chn_num);	
				goto error;		
			}			
			chncfg.status = channels[chncfg.chn_num].status;	
			ret = copy_to_user((void __user *)arg, &chncfg, sizeof(struct mvpdma_chncfg));
			if(ret){	
				printk("Copy to user failed\n");
				goto error;		
			}		
		}		
		break;		
		case AXIDMA_IOCRELEASECHN:		
		{			
			ret = copy_from_user(&chncfg, (void __user *)arg, sizeof(struct mvpdma_chncfg));
			if(ret){
				printk("Copy from user failed\n");	
				goto error;		
			}			
			if((chncfg.chn_num >= AXI_DMA_MAX_CHANS) || (!channels[chncfg.chn_num].dma_chan)){
				printk("chn_num[%d] is invalid\n", chncfg.chn_num);	
				goto error;			
			} 			
			dma_release_channel(channels[chncfg.chn_num].dma_chan);	
			channels[chncfg.chn_num].used = DMA_CHN_UNUSED;		
			channels[chncfg.chn_num].status = DMA_STATUS_UNFINISHED;
		}

		if(dst_addr){
			for(i=0; i<4096; i++){
				if(0x9 != *dst_addr++){
					printk("DMA COPY MEM failed!");
					kfree(src_addr);
					kfree(dst_addr);
					return -1;
				}
			}
		}
		
		printk("DMA COPY MEM ok!\n");
		
		kfree(src_addr);
		kfree(dst_addr);
		break;		
		default:			
			printk("Don't support cmd [%d]\n", cmd);		
			break;	
		}	
	return 0;
error:	
	return -EFAULT;
} 


/* *    Kernel Interfaces */ 
static struct file_operations mvpdma_fops = {
	.owner        = THIS_MODULE, 
	.llseek        = no_llseek,  
	.write        = mvpdma_write,
	.unlocked_ioctl = mvpdma_unlocked_ioctl,
	.open        = mvpdma_open,   
	.release    = mvpdma_release,
}; 


static struct miscdevice mvpdma_miscdev = { 
	.minor        = MISC_DYNAMIC_MINOR,  
	.name        = DRIVER_NAME,
	.fops        = &mvpdma_fops,
}; 


static int __init mvpdma_init(void)
{    
	int ret = 0;   
	ret = misc_register(&mvpdma_miscdev);
	if(ret) 
	{       
		printk (KERN_ERR "cannot register miscdev (err=%d)\n", ret);
		return ret;  
	}	
	memset(&channels, 0, sizeof(channels)); 
	return 0;
} 


static void __exit mvpdma_exit(void)
{       
	misc_deregister(&mvpdma_miscdev);
} 

module_init(mvpdma_init);
module_exit(mvpdma_exit);
MODULE_AUTHOR("blh");
MODULE_DESCRIPTION("Dma Driver");
MODULE_LICENSE("GPL");

