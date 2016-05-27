/* REVIEW TODO: Header for this file */

#ifndef GW20_HCFS_STAT_H_
#define GW20_HCFS_STAT_H_

/* REVIEW TODO: There are quite a few parameters here. Could we aggregate those
into a structure (easier to read and maintain)? */
int32_t get_hcfs_stat(int64_t *quota, int64_t *vol_usage, int64_t *cloud_usage,
		      int64_t *cache_total, int64_t *cache_used, int64_t *cache_dirty,
		      int64_t *pin_max, int64_t *pin_total,
		      int64_t *xfer_up, int64_t *xfer_down,
		      int32_t *cloud_stat, int32_t *data_transfer);

int32_t get_occupied_size(int64_t *occupied);

#endif  /* GW20_HCFS_STAT_H_ */
