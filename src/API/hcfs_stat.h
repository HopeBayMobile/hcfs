#ifndef GW20_HCFS_STAT_H_
#define GW20_HCFS_STAT_H_

int get_hcfs_stat(long long *vol_usage, long long *cloud_usage, long long *cache_total,
		  long long *cache_used, long long *cache_dirty,
		  long long *pin_max, long long *pin_total,
		  long long *xfer_up, long long *xfer_down,
		  int *cloud_stat);

#endif  /* GW20_HCFS_STAT_H_ */
