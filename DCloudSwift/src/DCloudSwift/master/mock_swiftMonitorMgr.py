
class SwiftMonitorMgr:
    """
    Mockup for SwiftMonitorMgr
    """
    
    def get_zone_info(self):
        """
        Get zone related infomations
        
        Essential columns:
        ip: node management console ip
        nodes: total number of node
        used: zone used storage percentage
        free: zone free storage percentage
        capacity: total zone capacity
        
        >>> SM = SwiftMonitorMgr()
        >>> print SM.get_zone_info()
        {'ip': '192.168.1.104', 'nodes': 3, 'used': '12.5', 'capacity': '12TB', 'repair':'3.5', 'free': '84'}
        """
        zone = {"ip":"192.168.1.104","nodes":3,"used":"12.5","repair":"3.5","free":"84","capacity":"12TB"}
        return zone

    def list_nodes_info(self):
        """
        Get all nodes infomations
        
        Essential columns:
        ip: node private ip
        index: enumerate number of the node
        hostname: node alias name
        status: node current stat (dead or alive)
        mode: node operation mode (waiting or service)
        capacity: raw capacity of the node
        hd_number: node's total hard disk number
        hd_error: node's total hard disk error number
        hd_ino: dictionary of each hard disk status in this node, which include:
            serial: serial number
            capacity: raw capacity of the hd
            status: operation status (OK or Broken or Missing)
        
        >>> SM = SwiftMonitorMgr()
        >>> print SM.list_nodes_info()
        [{'status': 'dead', 'index': '1', 'capacity': '12.0TB', 'mode': 'waiting', 'ip': '172.30.11.33', 'hostname': 'TPEIIA', 'hd_number': 6, 'hd_info': [{'status': 'Broken', 'serial': 'SN_TP02', 'capacity': '2.0TB'}, {'status': 'OK', 'serial': 'SN_TP03', 'capacity': '2.0TB'}, {'status': 'OK', 'serial': 'SN_TP04', 'capacity': '2.0TB'}], 'hd_error': 1}, {'status': 'alive', 'index': '2', 'capacity': '12.0TB', 'mode': 'waiting', 'ip': '172.30.11.37', 'hostname': 'TPEIIB', 'hd_number': 6, 'hd_info': [{'status': 'OK', 'serial': 'SN_TP02', 'capacity': '2.0TB'}, {'status': 'OK', 'serial': 'SN_TP03', 'capacity': '2.0TB'}, {'status': 'OK', 'serial': 'SN_TP04', 'capacity': '2.0TB'}], 'hd_error': 0}, {'status': 'alive', 'index': '3', 'capacity': '12.0TB', 'mode': 'service', 'ip': '172.30.11.25', 'hostname': 'TPEIIC', 'hd_number': 6, 'hd_info': [{'status': 'Missing', 'serial': 'N/A', 'capacity': '2.0TB'}, {'status': 'OK', 'serial': 'SN_TP03', 'capacity': '2.0TB'}, {'status': 'OK', 'serial': 'SN_TP04', 'capacity': '2.0TB'}], 'hd_error': 1}]
        """
        nodes_info = []
        nodes_info.append({"ip":"172.30.11.33","index":"1","hostname":"TPEIIA","status":"dead","mode":"waiting","hd_number":6,"hd_error":1,"capacity":"12.0TB",
        "hd_info":[{"serial":"SN_TP02","status":"Broken","capacity":"2.0TB"},
                   {"serial":"SN_TP03","status":"OK","capacity":"2.0TB"},
                   {"serial":"SN_TP04","status":"OK","capacity":"2.0TB"}]
        })
        nodes_info.append({"ip":"172.30.11.37","index":"2","hostname":"TPEIIB","status":"alive","mode":"waiting","hd_number":6,"hd_error":0,"capacity":"12.0TB", 
        "hd_info":[{"serial":"SN_TP02","status":"OK","capacity":"2.0TB"},
                   {"serial":"SN_TP03","status":"OK","capacity":"2.0TB"},
                   {"serial":"SN_TP04","status":"OK","capacity":"2.0TB"}]
        })
        nodes_info.append({"ip":"172.30.11.25","index":"3","hostname":"TPEIIC","status":"alive","mode":"service","hd_number":6,"hd_error":1,"capacity":"12.0TB",
        "hd_info":[{"serial":"N/A","status":"Missing","capacity":"2.0TB"},
                   {"serial":"SN_TP03","status":"OK","capacity":"2.0TB"},
                   {"serial":"SN_TP04","status":"OK","capacity":"2.0TB"}]
        })
        return nodes_info

if __name__ == '__main__':
    SM = SwiftMonitorMgr()
    print SM.get_zone_info()
    print SM.list_nodes_info()
