class HDD:
    event_name = "HDD"
    level_name = ["OK", "WARNING", "ERROR"]
    healthy = [True, False]

class HEARTBEAT:
    event_name = "HEARTBEAT"
    level_name = None
    status = ["alive", "unknown", "dead"]

EVENT_LIST = [HDD, HEARTBEAT]
