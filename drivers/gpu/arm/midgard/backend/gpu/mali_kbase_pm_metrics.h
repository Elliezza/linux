/*
 *
 * (C) COPYRIGHT 2011-2016 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */





/*
 * Metrics for power management
 */



#include <mali_kbase.h>
#include <mali_kbase_pm.h>
#include <backend/gpu/mali_kbase_pm_internal.h>
#include <backend/gpu/mali_kbase_jm_rb.h>

/* When VSync is being hit aim for utilisation between 70-90% */
#define KBASE_PM_VSYNC_MIN_UTILISATION          70
#define KBASE_PM_VSYNC_MAX_UTILISATION          90
/* Otherwise aim for 10-40% */
#define KBASE_PM_NO_VSYNC_MIN_UTILISATION       10
#define KBASE_PM_NO_VSYNC_MAX_UTILISATION       40

/* Shift used for kbasep_pm_metrics_data.time_busy/idle - units of (1 << 8) ns
 * This gives a maximum period between samples of 2^(32+8)/100 ns = slightly
 * under 11s. Exceeding this will cause overflow */
#define KBASE_PM_TIME_SHIFT			8

/* Maximum time between sampling of utilization data, without resetting the
 * counters. */
#define MALI_UTILIZATION_MAX_PERIOD 100000 /* ns = 100ms */

#ifdef CONFIG_MALI_MIDGARD_DVFS
static enum hrtimer_restart dvfs_callback(struct hrtimer *timer);
#endif /* CONFIG_MALI_MIDGARD_DVFS */

int kbasep_pm_metrics_init(struct kbase_device *kbdev);
//KBASE_EXPORT_TEST_API(kbasep_pm_metrics_init);

void kbasep_pm_metrics_term(struct kbase_device *kbdev);

/* caller needs to hold kbdev->pm.backend.metrics.lock before calling this
 * function
 */
static void kbase_pm_get_dvfs_utilisation_calc(struct kbase_device *kbdev,
								ktime_t now);
#if defined(CONFIG_PM_DEVFREQ) || defined(CONFIG_MALI_MIDGARD_DVFS)
/* Caller needs to hold kbdev->pm.backend.metrics.lock before calling this
 * function.
 */
static void kbase_pm_reset_dvfs_utilisation_unlocked(struct kbase_device *kbdev,
								ktime_t now);

void kbase_pm_reset_dvfs_utilisation(struct kbase_device *kbdev);

void kbase_pm_get_dvfs_utilisation(struct kbase_device *kbdev,
		unsigned long *total_out, unsigned long *busy_out);
#endif

#ifdef CONFIG_MALI_MIDGARD_DVFS

/* caller needs to hold kbdev->pm.backend.metrics.lock before calling this
 * function
 */
int kbase_pm_get_dvfs_utilisation_old(struct kbase_device *kbdev,
					int *util_gl_share,
					int util_cl_share[2],
					ktime_t now);
void kbase_pm_get_dvfs_action(struct kbase_device *kbdev);
bool kbase_pm_metrics_is_active(struct kbase_device *kbdev);
//KBASE_EXPORT_TEST_API(kbase_pm_metrics_is_active);

#endif /* CONFIG_MALI_MIDGARD_DVFS */

/**
 * kbase_pm_metrics_active_calc - Update PM active counts based on currently
 *                                running atoms
 * @kbdev: Device pointer
 *
 * The caller must hold kbdev->pm.backend.metrics.lock
 */
static void kbase_pm_metrics_active_calc(struct kbase_device *kbdev);
/* called when job is submitted to or removed from a GPU slot */
void kbase_pm_metrics_update(struct kbase_device *kbdev, ktime_t *timestamp);
