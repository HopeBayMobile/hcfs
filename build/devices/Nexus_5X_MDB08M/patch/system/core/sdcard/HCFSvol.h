
#ifndef GW20_SRC_HCFSVOL_H_
#define GW20_SRC_HCFSVOL_H_

#define MAX_FILENAME_LEN 255
#ifdef _ANDROID_ENV_
#define ANDROID_INTERNAL 1
#define ANDROID_EXTERNAL 2
#endif

typedef struct {
        ino_t d_ino;
        char d_name[MAX_FILENAME_LEN+1];
        char d_type;
} DIR_ENTRY;

#endif  /* GW20_SRC_HCFSVOL_H_ */

int hcfsvol(int, char*, char);
