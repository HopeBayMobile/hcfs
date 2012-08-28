def human_readable_capacity(capacity):
    human_int = capacity/1000000
    human_int = capacity/1000000
    human_float = (capacity - (human_int*1000000))/10000
    if(human_float<10):
        append_float="0"+str(human_float)
    else:
        append_float=str(human_float)
    return str(human_int) + "." + append_float +"G"


