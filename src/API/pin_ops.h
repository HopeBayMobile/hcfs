#ifndef GW20_PIN_API_H_
#define GW20_PIN_API_H_

int pin_by_path(char *buf, unsigned int arg_len);

int unpin_by_path(char *buf, unsigned int arg_len);

int check_pin_status(char *buf, unsigned int arg_len);

int check_file_loc(char *buf, unsigned int arg_len);

#endif  /* GW20_PIN_API_H_ */
