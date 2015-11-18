#ifndef GW20_HCFS_STAT_H_
#define GW20_HCFS_STAT_H_

int get_cloud_usage(long long *cloud_usage);

int get_cache_usage(long long *cache_total, long long *cache_used,
		    long long *cache_dirty);

int get_pin_usage(long long *pin_max, long long *pin_total);

#endif  /* GW20_HCFS_STAT_H_ */
