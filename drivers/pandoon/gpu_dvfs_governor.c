/* drivers/gpu/t6xx/kbase/src/platform/gpu_dvfs_governor.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_dvfs_governor.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/io.h>
#include <linux/pm_qos.h>
#include <mach/asv-exynos.h>

#include "mali_kbase_platform.h"
#include "mali_kbase_config_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#ifdef CONFIG_CPU_THERMAL_IPA
#include "gpu_ipa.h"
#endif /* CONFIG_CPU_THERMAL_IPA */

#include <linux/gpandoon.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/delay.h>
#include <linux/cpufreq_pandoon.h>
#include <linux/proc_fs.h>
#include<linux/slab.h>                 //kmalloc()
#include<linux/uaccess.h>              //copy_to/from_user()
//#include "governor.h"
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/kthread.h>

#include <linux/random.h>
wait_queue_head_t wq;
int flag = 0;

static struct task_struct *wait_thread;


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

	}
	return 0;


}





#ifdef CONFIG_MALI_MIDGARD_DVFS
typedef void (*GET_NEXT_FREQ)(struct kbase_device *kbdev, int utilization);
GET_NEXT_FREQ gpu_dvfs_get_next_freq;

static char *governor_list[G3D_MAX_GOVERNOR_NUM] = {"Default", "Static", "Booster","Pandoon"};
#endif /* CONFIG_MALI_MIDGARD_DVFS */

#define GPU_DVFS_TABLE_SIZE(X)  ARRAY_SIZE(X)
#define CPU_MAX PM_QOS_CPU_FREQ_MAX_DEFAULT_VALUE

static gpu_dvfs_info gpu_dvfs_infotbl_default[] = {
/*  vol,clk,min,max,down stay, pm_qos mem, pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
	{812500,  177,  0,   3, 2, 0, 275000, 222000,       0, CPU_MAX},
	{862500,  266,  4,   6, 1, 0, 413000, 222000,       0, CPU_MAX},
	{912500,  350,  7,  10, 1, 0, 728000, 333000,       0, CPU_MAX},
	{962500,  420, 11,  14, 1, 0, 825000, 400000,       0, CPU_MAX},
	{1000000, 480, 15,  20, 1, 0, 825000, 400000, 1000000, CPU_MAX},
	{1037500, 543, 21,  25, 1, 0, 825000, 400000, 1000000, CPU_MAX},
	{1150000, 600, 26, 100, 1, 0, 825000, 413000, 1500000, CPU_MAX},
};


#ifdef CONFIG_DYNIMIC_ABB
static int gpu_abb_infobl_default[] = {900000, 900000, 950000, 1000000, 1075000, 1175000};
#endif 

#ifdef CONFIG_MALI_MIDGARD_DVFS
static int gpu_dvfs_governor_default(struct kbase_device *kbdev, int utilization)
{
	struct exynos_context *platform;
	/* HACK: On Linux es2gears and glmark2-es2 doesn't utilize 100% 
	 * of the GPU. So we need to keep it on the high frequency to get 
	 * proper performance. */
	utilization = 100;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if ((platform->step < platform->table_size-1) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step++;
		platform->down_requirement = platform->table[platform->step].stay_count;
		DVFS_ASSERT(platform->step < platform->table_size);
	} else if ((platform->step > 0) && (utilization < platform->table[platform->step].min_threshold)) {
		DVFS_ASSERT(platform->step > 0);
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step--;
			platform->down_requirement = platform->table[platform->step].stay_count;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].stay_count;
	}

	return 0;
}

//struct kbase_device *gkd;
//bool gset;
int tfunc(void *input){
	struct kbase_device *kd=(struct kbase_device*)input;
	struct exynos_context * platform;
	platform=(struct exynos_context *) kd->platform_context;
	//int i=0;
	//if(!gset){
	//	gset=1;
	//	gkd=kd;
	//}
	/*while(true){
		printk("platform step:%d",platform->step);
		platform->step=i;
		i++;
		i=i%5;
		udelay(2000);
	}*/
	


	//struct devfreq *df=(struct devfreq*) input;
	//u32 flags=0;
	//int i=1;
	int index=0;
	int index1=0;
	int index2=0;
	unsigned long f1=237143000;
	//long unsigned int f2=767000000;
	unsigned int freq_req1,freq_req2;	
	//struct cpufreq_frequency_table *freq_table1;
	//struct cpufreq_frequency_table *freq_table2; 
	unsigned int freq_table1[]={200000,300000,400000,500000,600000,700000,800000,900000,1000000,1100000,1200000,1300000,1400000,1500000,1600000,1700000,1800000,1900000,2000000};
	unsigned int freq_table2[]={200000,300000,400000,500000,600000,700000,800000,900000,1000000,1100000,1200000,1300000,1400000};
	printk("salaaam\n");
	int i=0;
	for (i = 0; i < platform->table_size; i++) {
                printk("%d,%u\n",i,platform->table[i].clock);
                        
        }

	if(psett2 && psett1){
		printk("hiii\n");
	//	freq_table1=globpolicy1->freq_table;
	//	freq_table2=globpolicy2->freq_table;
		while(true){
			
			//f1=df->profile->freq_table[index++];
			freq_req1 = freq_table1[index1];
			freq_req2 = freq_table2[index2];
			////df->profile->target(df->dev.parent, &f1, flags);
			platform->step=index;
			__cpufreq_driver_target(globpolicy1, freq_req1, CPUFREQ_RELATION_H);
			__cpufreq_driver_target(globpolicy2, freq_req2, CPUFREQ_RELATION_H);
			freqs.gf=platform->cur_clock;
			freqs.f1=freq_req1;
		        freqs.f2=freq_req2;
			//freqs.capturing=cap;
			//printk("gpuindex:%d,index1:%d,index2:%d\n",index,index1,index2);
			//printk("frequency setting to %lu, %u,%u\n",freqs.gf,freq_req1,freq_req2);
			get_random_bytes(&index, sizeof(int)-1);
			get_random_bytes(&index1, sizeof(int)-1);
			get_random_bytes(&index2, sizeof(int)-1);
			index=index%8;
			index1=index1%19;
			index2=index2%13;
			//udelay(16000);
			wait_event_interruptible(wq, flag != 0);
			if(flag==2){
				//index1=f_index;
				//index1=index1%9;
				index1=f_index%100;
				index2=(f_index/100);
				index2=index2%100;
				index=(f_index/10000)-1;
				
			}
			//freqs.gf=platform->cur_clock;
			//freqs.f1=freq_req1;
			//freqs.f2=freq_req2;			
                        printk("gpuindex:%d,index1:%d,index2:%d\n",index,index1,index2);
                        printk("frequency setting to %lu, %u,%u\n",freqs.gf,freq_req1,freq_req2);

			flag=0;
		}

	}
	return 0;
}


static int gpu_dvfs_governor_pandoon(struct kbase_device *kbdev, int utilization)
{



    static bool first=1;
	if (first){
		proc_create("pandoon",0666,NULL,&proc_fops);	
		init_waitqueue_head(&wq);
		freqs.capturing=false;
		first=0;
		//pthread_t pt;
		wait_thread=kthread_create(tfunc,(void*)kbdev,"wthread");
		if (wait_thread) {
	                printk("Thread Created successfully\n");
        	        wake_up_process(wait_thread);
        	} else
                	printk(KERN_INFO "Thread creation failed\n");
		return 0;
	}else
		return 0;



	////struct exynos_context *platformi;
	/* HACK: On Linux es2gears and glmark2-es2 doesn't utilize 100% 
	 * of the GPU. So we need to keep it on the high frequency to get 
	 * proper performance. */
	/*utilization = 100;
	static int dir=0;
	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if ((platform->step < platform->table_size-1)) {
		platform->step++;
		//platform->down_requirement = platform->table[platform->step].stay_count;
		//DVFS_ASSERT(platform->step < platform->table_size);
	} else {
		platform->down_requirement = platform->table[platform->step].stay_count;
		platform->step--;
	}

	return 0;*/
}



static int gpu_dvfs_governor_static(struct kbase_device *kbdev, int utilization)
{
	struct exynos_context *platform;
	static bool increase = true;
	static int count;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	if (count == G3D_GOVERNOR_STATIC_PERIOD) {
		if (increase) {
			if (platform->step < platform->table_size-1)
				platform->step++;
			if (((platform->max_lock > 0) && (platform->table[platform->step].clock == platform->max_lock))
					|| (platform->step == platform->table_size-1))
				increase = false;
		} else {
			if (platform->step > 0)
				platform->step--;
			if (((platform->min_lock > 0) && (platform->table[platform->step].clock == platform->min_lock))
					|| (platform->step == 0))
				increase = true;
		}

		count = 0;
	} else {
		count++;
	}

	return 0;
}

static int gpu_dvfs_governor_booster(struct kbase_device *kbdev, int utilization)
{
	struct exynos_context *platform;
	static int weight;
	int cur_weight, booster_threshold, dvfs_table_lock, i;

	platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	cur_weight = platform->cur_clock*utilization;
	/* booster_threshold = current clock * set the percentage of utilization */
	booster_threshold = platform->cur_clock * 50;

	dvfs_table_lock = platform->table_size-1;
	for (i = platform->table_size-1; i >= 0; i--)
		if (platform->table[i].max_threshold == 100)
			dvfs_table_lock = i;

	if ((platform->step < dvfs_table_lock-2) &&
			((cur_weight - weight) > booster_threshold)) {
		platform->step += 2;
		platform->down_requirement = platform->table[platform->step].stay_count;
		GPU_LOG(DVFS_WARNING, "[G3D_booster] increase G3D level 2 step\n");
		DVFS_ASSERT(platform->step < platform->table_size);
	} else if ((platform->step < platform->table_size-1) &&
			(utilization > platform->table[platform->step].max_threshold)) {
		platform->step++;
		platform->down_requirement = platform->table[platform->step].stay_count;
		DVFS_ASSERT(platform->step < platform->table_size);
	} else if ((platform->step > 0) && (utilization < platform->table[platform->step].min_threshold)) {
		DVFS_ASSERT(platform->step > 0);
		platform->down_requirement--;
		if (platform->down_requirement == 0) {
			platform->step--;
			platform->down_requirement = platform->table[platform->step].stay_count;
		}
	} else {
		platform->down_requirement = platform->table[platform->step].stay_count;
	}
	weight = cur_weight;

	return 0;
}
#endif /* CONFIG_MALI_MIDGARD_DVFS */

static int gpu_dvfs_update_asv_table(struct exynos_context *platform, int governor_type)
{
	int i, voltage;
#ifdef CONFIG_DYNIMIC_ABB
	unsigned int asv_abb = 0;
	for (i = 0; i < platform->table_size; i++) {
		asv_abb = get_match_abb(ID_G3D, platform->table[i].clock*1000);
		if (!asv_abb) {
			platform->devfreq_g3d_asv_abb[i] = ABB_BYPASS;
		} else {
			platform->devfreq_g3d_asv_abb[i] = asv_abb;
		}
		GPU_LOG(DVFS_INFO, "DEVFREQ(G3D) : %uKhz, ABB %u\n", platform->table[i].clock*1000, platform->devfreq_g3d_asv_abb[i]);
#else
	for (i = 0; i < platform->table_size; i++) {
#endif
		voltage = get_match_volt(ID_G3D, platform->table[i].clock*1000);
		if (voltage > 0)
			platform->table[i].voltage = voltage;
		GPU_LOG(DVFS_INFO, "G3D %dKhz ASV is %duV\n", platform->table[i].clock*1000, platform->table[i].voltage);
	}
	return 0;
}

int gpu_dvfs_governor_init(struct kbase_device *kbdev, int governor_type)
{
	unsigned long flags;
#ifdef CONFIG_MALI_MIDGARD_DVFS
	int i, total = 0;
#endif /* CONFIG_MALI_MIDGARD_DVFS */
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);

#ifdef CONFIG_MALI_MIDGARD_DVFS
	switch (governor_type) {
	case G3D_DVFS_GOVERNOR_DEFAULT:
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_default;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
#ifdef CONFIG_DYNIMIC_ABB
		platform->devfreq_g3d_asv_abb = gpu_abb_infobl_default;
#endif
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_DEFAULT);
		break;
	case G3D_DVFS_GOVERNOR_STATIC:
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_static;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
#ifdef CONFIG_DYNIMIC_ABB
		platform->devfreq_g3d_asv_abb = gpu_abb_infobl_default;
#endif
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_STATIC);
		break;
	case G3D_DVFS_GOVERNOR_BOOSTER:
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_booster;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
#ifdef CONFIG_DYNIMIC_ABB
		platform->devfreq_g3d_asv_abb = gpu_abb_infobl_default;
#endif
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_BOOSTER);
		break;
	
	
	case G3D_DVFS_GOVERNOR_PANDOON:
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_pandoon;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
#ifdef CONFIG_DYNIMIC_ABB
		platform->devfreq_g3d_asv_abb = gpu_abb_infobl_default;
#endif
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_PANDOON);
		break;
	
	
	default:
		GPU_LOG(DVFS_WARNING, "[gpu_dvfs_governor_init] invalid governor type\n");
		gpu_dvfs_get_next_freq = (GET_NEXT_FREQ)&gpu_dvfs_governor_default;
		platform->table = gpu_dvfs_infotbl_default;
		platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
#ifdef CONFIG_DYNIMIC_ABB
		platform->devfreq_g3d_asv_abb = gpu_abb_infobl_default;
#endif
		platform->step = gpu_dvfs_get_level(platform, G3D_GOVERNOR_DEFAULT_CLOCK_DEFAULT);
		break;
	}

	platform->utilization = 100;
	platform->target_lock_type = -1;
	platform->max_lock = 0;
	platform->min_lock = 0;
#ifdef CONFIG_CPU_THERMAL_IPA
	gpu_ipa_dvfs_calc_norm_utilisation(kbdev);
#endif /* CONFIG_CPU_THERMAL_IPA */
	for (i = 0; i < NUMBER_LOCK; i++) {
		platform->user_max_lock[i] = 0;
		platform->user_min_lock[i] = 0;
	}

	platform->down_requirement = 1;
	platform->wakeup_lock = 0;

	platform->governor_type = governor_type;
	platform->governor_num = G3D_MAX_GOVERNOR_NUM;

	for (i = 0; i < G3D_MAX_GOVERNOR_NUM; i++)
		total += snprintf(platform->governor_list+total,
			sizeof(platform->governor_list), "[%d] %s\n", i, governor_list[i]);

	gpu_dvfs_init_time_in_state(platform);
#else
	platform->table = gpu_dvfs_infotbl_default;
	platform->table_size = GPU_DVFS_TABLE_SIZE(gpu_dvfs_infotbl_default);
#ifdef CONFIG_DYNIMIC_ABB
	platform->devfreq_g3d_asv_abb = gpu_abb_infobl_default;
#endif /* SOC_NAME */
	platform->step = gpu_dvfs_get_level(platform, MALI_DVFS_START_FREQ);
#endif /* CONFIG_MALI_MIDGARD_DVFS */

	platform->cur_clock = platform->table[platform->step].clock;

	/* asv info update */
	gpu_dvfs_update_asv_table(platform, governor_type);

	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 1;
}

#ifdef CONFIG_MALI_MIDGARD_DVFS
int gpu_dvfs_init_time_in_state(struct exynos_context *platform)
{
#ifdef CONFIG_MALI_MIDGARD_DEBUG_SYS
	int i;

	for (i = 0; i < platform->table_size; i++)
		platform->table[i].time = 0;
#endif /* CONFIG_MALI_MIDGARD_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_update_time_in_state(struct exynos_context *platform, int freq)
{
#ifdef CONFIG_MALI_MIDGARD_DEBUG_SYS
	u64 current_time;
	static u64 prev_time;
	int level = gpu_dvfs_get_level(platform, freq);

	if (prev_time == 0)
		prev_time = get_jiffies_64();

	current_time = get_jiffies_64();
	if ((level >= 0) && (level < platform->table_size))
		platform->table[level].time += current_time-prev_time;

	prev_time = current_time;
#endif /* CONFIG_MALI_MIDGARD_DEBUG_SYS */

	return 0;
}

int gpu_dvfs_decide_next_level(struct kbase_device *kbdev, int utilization)
{
	unsigned long flags;
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	spin_lock_irqsave(&platform->gpu_dvfs_spinlock, flags);
	gpu_dvfs_get_next_freq(kbdev, utilization);
	spin_unlock_irqrestore(&platform->gpu_dvfs_spinlock, flags);

	return 0;
}
#endif /* CONFIG_MALI_MIDGARD_DVFS */

int gpu_dvfs_get_level(struct exynos_context *platform, int freq)
{
	int i;

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == freq)
			return i;
	}

	return -1;
}

int gpu_dvfs_get_voltage(struct exynos_context *platform, int freq)
{
	int i;

	for (i = 0; i < platform->table_size; i++) {
		if (platform->table[i].clock == freq)
			return platform->table[i].voltage;
	}

	return -1;
}
