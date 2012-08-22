
class SwiftMonitorMgr:
    """
    Mockup for SwiftMonitorMgr
    """
    
    def get_zone_info(self):
        """
        Get zone related infomations
        
        >>> SA = SwiftMonitorMgr()
        >>> print SM.get_zone_info()
        {'ip': '192.168.1.104', 'nodes': 3, 'used': '21', 'capacity': '12TB', 'free': '79'}
        """
        zone = {"ip":"192.168.1.104","nodes":3,"used":"21","free":"79","capacity":"12TB"}
        return zone

    def list_nodes_info(self):
        """
        Get all nodes infomations
        """
        nodes_info = []
        nodes_info.append({"ip":"172.30.11.33","index":"1","hostname":"TPEIIA","status":"dead","mode":"waiting","hd_number":6,"hd_error":1,
        "hd_info":[{"serial":"SN_TP02","status":"Broken"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
        })
        nodes_info.append({"ip":"172.30.11.37","index":"2","hostname":"TPEIIB","status":"alive","mode":"waiting","hd_number":6,"hd_error":0,
        "hd_info":[{"serial":"SN_TP02","status":"OK"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
        })
        nodes_info.append({"ip":"172.30.11.25","index":"3","hostname":"TPEIIC","status":"alive","mode":"service","hd_number":6,"hd_error":0,
        "hd_info":[{"serial":"SN_TP02","status":"OK"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
        })
        return nodes_info

if __name__ == '__main__':
    SA = SwiftMonitorMgr()
    print SM.get_zone_info()