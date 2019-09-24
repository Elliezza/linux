/*
 *  linux/drivers/cpufreq/cpufreq_powersave.c
 *
 *  Copyright (C) 2002 - 2003 Dominik Brodowski <linux@brodo.de>
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpufreq.h>
#include <linux/init.h>

////
//#include "governor.h"
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>
#include <linux/gpandoon.h>
#include <linux/random.h>

#include <linux/delay.h>
#include "cpu_load_metric.h"
/*
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/delay.h>
#include <linux/cpufreq_pandoon.h>
#include <linux/proc_fs.h>
#include<linux/slab.h>                 //kmalloc()
#include<linux/uaccess.h>              //copy_to/from_user()

#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>

#include <linux/random.h>

wait_queue_head_t wq;
int flag = 0;
*/ 
static struct task_struct *wait_thread;

#include "cpufreq_governor.h"

/*
struct frequencies{
        long unsigned int gf;
        unsigned int f1;
        unsigned int f2;
	bool capturing;
};
//bool cap=false;
#define next_state _IO('p','n')
#define capture_freqs _IOR('p','c',struct frequencies *)
#define next_freq _IOW('p','f', int*)
#define capture_f _IOR('p','a',unsigned int*)




struct frequencies freqs;
char value = '0';
//char etx_array[20]="try_proc_array\n";
static int len = 1;
int f_index=0;

static ssize_t read_proc(struct file *filp, char __user *buffer, size_t length,loff_t * offset);
static ssize_t write_proc(struct file *filp, const char *buff, size_t len, loff_t * off);
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

static struct file_operations proc_fops = {
       // .open = open_proc,
        .read = read_proc,
        .write = write_proc,
        .unlocked_ioctl=my_ioctl,
       // .release = release_proc
};




static ssize_t read_proc(struct file *filp, char __user *buffer, size_t length,loff_t * offset)
{
    printk(KERN_INFO "proc file read.....\n");
    if(len)
        len=0;
    else{
        len=1;
        return 0;
    }
    copy_to_user( buffer, &value, sizeof(value));
 
    return length;;
}
 
static ssize_t write_proc(struct file *filp, const char *buff, size_t len, loff_t * off)
{
    printk(KERN_INFO "proc file wrote.....\n");
    copy_from_user(&value,buff,sizeof(value));
    printk(KERN_INFO "value=%c\n",value);
    if (value=='0')
	freqs.capturing=false;
    if (value=='1')
	freqs.capturing=true;
    return len;
}

unsigned int freq_req;

static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
	switch(cmd){
		case next_state:
			flag = 1 ;
			wake_up_interruptible (&wq);
			break;
		
		case next_freq:
			flag=2;
			copy_from_user(&f_index, (int*)arg, sizeof(int));
			wake_up_interruptible (&wq);
			break;
		case capture_freqs:
			copy_to_user((struct frequencies*) arg, &freqs, sizeof(freqs));
			break;
	
		case capture_f:
			copy_to_user((unsigned int*) arg, &freq_req, sizeof(freq_req));
			break;
	}
	return 0;


}

struct cpufreq_policy *globpolicy;
bool pset1;


int pandoon(void *input){
	
	while(!gpuset)
		printk("gpunotset\n");
	unsigned long f1=0;
	unsigned int freq_req1,freq_req2;
	u32 flags=0;
	int index=0;
	int index1=0;
	int index2=0;
	struct cpufreq_policy *p=(struct cpufreq_policy *)input;
	//struct cpufreq_frequency_table *freq_table1=cpufreq_frequency_get_table(4);	
	//struct cpufreq_frequency_table *freq_table2=cpufreq_frequency_get_table(0);
	unsigned int freq_table1[]={200000,300000,400000,500000,600000,700000,800000,900000,1000000,1100000,1200000,1300000,1400000,1500000,1600000,1700000,1800000,1900000,2000000};
	unsigned int freq_table2[]={200000,300000,400000,500000,600000,700000,800000,900000,1000000,1100000,1200000,1300000,1400000};	
	////unsigned long gpu_tablee[]={138000,165000,206000,275000,413000,543000,633000,728000,825000};
	//unsigned int *gpu_table=dfreq->profile->freq_table;
	struct exynos_context * platform;
        platform=(struct exynos_context *) gkd->platform_context;
	while(true){
		////f1=gpu_tablee[index];
		//freq_req1 = freq_table1[index1].frequency;
		//freq_req2 = freq_table2[index2].frequency;
		freq_req1=freq_table1[index1];
		freq_req2=freq_table2[index2];
		////dfreq->profile->target(dfreq->dev.parent, &f1, flags);
		platform->step=index;
		__cpufreq_driver_target(globpolicy, freq_req1, CPUFREQ_RELATION_H);
		__cpufreq_driver_target(p, freq_req2, CPUFREQ_RELATION_H);
		freqs.gf=f1;
		freqs.f1=freq_req1;
		freqs.f2=freq_req2;
		//freqs.capturing=cap;
		get_random_bytes(&index, sizeof(int)-1);
		get_random_bytes(&index1, sizeof(int)-1);
		get_random_bytes(&index2, sizeof(int)-1);
		index=index%5;
		index1=index1%19;
		index2=index2%13;
		//udelay(16000);
		wait_event_interruptible(wq, flag != 0);
		if(flag==2){
			index=f_index%10;
			index2=f_index/10;
			index2=index2%10;
			index1=f_index/100;
		}
		flag=0;

		//dbs_info = &per_cpu(od_cpu_dbs_info,policy->cpu);
		//unsigned int freq_req;
		//freq_req = dbs_info->freq_table[index].frequency;
	
		
	}
	return 0;
}


*/

//#include "cpufreq_pandoon.h"
bool psett1=0;
bool psett2=0;

struct cpufreq_policy *globpolicy1;
struct cpufreq_policy *globpolicy2;


/*
/// Ehsan:   wall time(Total time) and idle time for all of 8 cores
u64 prev_cpu_wall[8]={0,0,0,0,0,0,0,0};//or now
u64 prev_cpu_idle[8]={0,0,0,0,0,0,0,0};
int ufunc(void* input){

	//printk("see meeee\n");
	struct dbs_data *dbs_data1 = globpolicy1->governor_data;
	struct dbs_data *dbs_data2 = globpolicy2->governor_data;
	//printk("dbs_data ok!\n");
	while(true){
		unsigned int max_load = 0;
        	//unsigned int ignore_nice;
	        unsigned int j;
		for_each_cpu(j, globpolicy1->cpus) {
			//printk("start of for:%d\n",j);
        	       // struct cpu_dbs_common_info *j_cdbs;
	                u64 cur_wall_time, cur_idle_time;
        	        unsigned int idle_time, wall_time;
	                unsigned int load;
        	        int io_busy = 0;
			//printk("before suspecious\n");
	                //j_cdbs = dbs_data1->cdata->get_cpu_cdbs(j);
			//printk("one\n");
			// Ehsan:  current idle time(accumulative) /
			cur_idle_time = get_cpu_idle_time(j, &cur_wall_time, io_busy);
			//printk("cur_idle_time:%llu,cur_wall_time:%llu\n",cur_idle_time,cur_wall_time);
			//Ehsan: calculate wall(elapsed) time from previous call //
			wall_time = (unsigned int)
                	        (cur_wall_time - prev_cpu_wall[j]);
			//printk("wall_time:%u,prev_cpu_wall:%llu\n",wall_time,prev_cpu_wall[j]);
			//printk("two\n");
			// Ehsan:  update last wall time //
        	        prev_cpu_wall[j] = cur_wall_time;
			//printk("here\n");
			// Ehsan:   calculate idle time from previous call//
	                idle_time = (unsigned int)
                	        (cur_idle_time - prev_cpu_idle[j]);
			//printk("idle_time:%u,prev_cpu_idle:%llu\n",idle_time,prev_cpu_idle[j]);
			// Ehsan:   update previous idle time//
        	        prev_cpu_idle[j] = cur_idle_time;
	                if (unlikely(!wall_time || wall_time < idle_time)){
				//printk("unlikely\n");
        	                continue;
			}
			// Ehsan:   calculate load//
        	        load = 100 * (wall_time - idle_time) / wall_time;
			//printk("load is :%u\n",load);
	                if (load > max_load)
                        	max_load = load;
			//printk("until update metrics\n");
                	////update_cpu_metric(j, cur_wall_time, idle_time, wall_time, globpolicy1);
			//printk("updated\n");
	        }

	        //dbs_data1->cdata->gov_check_cpu(cpu, max_load);
		printk("maxload is:%u\n",max_load);
		mdelay(100);
	}
	return 0;
}

*/
static int cpufreq_governor_pandoon(struct cpufreq_policy *policy,
					unsigned int event)
{

	/* Ehsan:   we should set globpolicy1 and globpolicy2 for little and big core, the first gover*/

	//static bool first=1;
	if(psett1==0){
		globpolicy1=policy;	
		psett1=1;
		return 0;
	}
	if(psett1==1 && psett2==0){
		if(policy==globpolicy1)
			return 0;
		globpolicy2=policy;
		psett2=1;
		//wait_thread=kthread_create(ufunc,(void*)policy,"wthread");
		//if (wait_thread) {
	          //      printk("Utilization Thread Created successfully\n");
        	    //    wake_up_process(wait_thread);
        //	} else
          //      	printk(KERN_INFO "Thread creation failed\n");
		return 0;	
	}
	return 0;
/*
	if (first){
		if(policy==globpolicy)
			return 0;
		first=0;
		//pthread_t pt;
		wait_thread=kthread_create(pandoon,(void*)policy,"wthread");
		if (wait_thread) {
	                printk("Thread Created successfully\n");
        	        wake_up_process(wait_thread);
        	} else
                	printk(KERN_INFO "Thread creation failed\n");
		return 0;
	}else
		return 0;

*/
}

	

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_PANDOON
static
#endif
struct cpufreq_governor cpufreq_gov_pandoon = {
	.name		= "pandoon",
	.governor	= cpufreq_governor_pandoon,
	.owner		= THIS_MODULE,
};

static int __init cpufreq_gov_pandoon_init(void)
{
	//proc_create("pandoon",0666,NULL,&proc_fops);
	//init_waitqueue_head(&wq);
	return cpufreq_register_governor(&cpufreq_gov_pandoon);
}


static void __exit cpufreq_gov_pandoon_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_pandoon);
}


MODULE_AUTHOR("Dominik Brodowski <linux@brodo.de>");
MODULE_DESCRIPTION("CPUfreq policy governor 'pandoon'");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_PANDOON
fs_initcall(cpufreq_gov_pandoon_init);
#else
module_init(cpufreq_gov_pandoon_init);
#endif
module_exit(cpufreq_gov_pandoon_exit);
