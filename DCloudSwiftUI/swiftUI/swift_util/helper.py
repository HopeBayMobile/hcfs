def human_readable_capacity(capacity):
    human_int = capacity/(1024*1024*1024)
    human_float = (capacity - (human_int*(1024*1024*1024)))/(1024*1024)
    #remain 1 digits
    if(human_float<10):
        append_float="0"
    else:
        append_float=str(human_float/10)
    return str(human_int) + "." + append_float +"G"
 
