def enum(**enums):
    return type('Enum', (), enums)

class HDD:
    event_name = "HDD"
    level_name = enum(OK="OK", WARNING="WARNING", ERROR="ERROR")
    healthy = enum(YES=True, NO=False)

class HEARTBEAT:
    event_name = "HEARTBEAT"
    level_name = None
    status = enum(ALIVE="alive", UNKNOWN="unknown", DEAD="dead")

EVENT_LIST = [HDD, HEARTBEAT]
