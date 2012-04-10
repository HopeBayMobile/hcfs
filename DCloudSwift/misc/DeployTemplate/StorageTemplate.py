'''
This sample code is created by GUI team.
Modified by CW to fit the requirement of Storage Appliance

First Modification by CW on 2012/02/07
'''

from task_worker import task, task_class
from task_worker import WorkerTask
import json
import time
import logging
import sys
sys.path.append('/etc/DCloud/DCloudGfs/src/GlusterfsMgt')
from GlusterfsMgt import volumeCreate, replaceServer, triggerSelfHealing 
# this is a sample code for using task_worker
# use decorator @task and @task_class
# to so that it will be registered as callbale worker task
# use decorator @task to covert your function into worker task

storage_meta = {
"purpose": "deploy",

"name": "Storage Appliance",
"base_os": ["Ubuntu 11.04 x64", "CentOS 6.1 x64"],
"min_host": 1,
"max_host": 160,

"user_config":{

	"adminn_name": {
	"description": "The Admin Name",
	"type": "text",
	"max": 10,
	"min": 3,
	"required": True,
	"default": "delta",
	"order": 0
	},
	"project_name": {
	"description": "The Project Name",
	"type": "text",
	"max": 10,
	"min": 3,
	"required": True,
	"default": "myproject",
	"order": 1
	},
	"network_type": {
	"description": "Network Type",
	"type": "radio",
	"required": False,
	"value": ["nova.network.manager.FlatManager", "nova.network.manager.FlatDHCPManager", "nova.network.manager.VlanManager"],
	"default": "nova.network.manager.FlatManager",
	"order": 2
	},
	"fixed_ips": {
	"description": "The Virtual Machine Private Network Range",
	"type": "text",
	"max": 20,
	"min": 3,
	"required": False,
	"default": "192.168.11.0/8",
	"order": 3
	},
	"floating_ips": {
	"description": "The Virtual Machine Public Network Range",
	"type": "text",
	"max": 20,
	"min": 3,
	"required": False,
	"default": "192.168.11.0/24",
	"order": 4
	},
	"bridge_interface": {
	"description": "Bridge Interface",
	"type": "text",
	"max": 15,
	"min": 3,
	"required": False,
	"default": "br100",
	"order": 5
	},
	"num_netowrks": {
	"description": "Number of Netowrks",
	"type": "text",
	"max": 5,
	"min": 1,
	"required": True,
	"default": "1",
	"order": 6
	},
	"network_size": {
	"description": "The Private Network Size",
	"type": "text",
	"max": 5,
	"min": 1,
	"required": True,
	"default": "256",
	"order": 7
	},
	"libvirt_type": {
	"description": "The Virtualization Type",
	"type": "text",
	"max": 15,
	"min": 3,
	"required": False,
	"default": "qemu",
	"order": 8
	},
	"share_volume_name": {
	"description": "Share Volume Name",
	"type": "text",
	"max": 100,
	"min": 3,
	"required": True,
	"default": "StorageVolume",
	"order": 9
	},
	"share_volume_type": {
	"description": "Share Volume Type",
	"type": "radio",
	"required": True,
	"value": ["distribute", "replica"],
	"default": "replica",
	"order": 10
	},
	"share_volume_count": {
	"description": "Share Volume Count",
	"type": "radio",
	"required": True,
	"value": ["2", "3", "4", "5"],
	"default": "2",
	"order": 11
	},
	"mysql_password": {
	"description": "MySQL Password",
	"type": "text",
	"max": 15,
	"min": 3,
	"required": True,
	"default": "nova",
	"order": 12
	},
	"ssh_user": {
	"description": "SSH User Name(for Deploy OpenStack)",
	"type": "text",
	"max": 15,
	"min": 3,
	"required": True,
	"default": "nii",
	"order": 13
	},
	"ssh_cmd_prefix": {
	"description": "SSH Prefix Command",
	"type": "text",
	"max": 100,
	"min": 3,
	"required": True,
	"default": "ssh -o StrictHostKeyChecking=no -t nii",
	"order": 14
	},
	"network_interface": {
	"description": "Network Interface",
	"type": "text",
	"max": 15,
	"min": 3,
	"required": True,
	"default": "eth1",
	"order": 15
	},

}
}

@task_class
class StorageMeta(WorkerTask):
    meta = storage_meta

    def run(self, deploy_json):
        # this is where function call starts
        # a test function, update 10% progess every 30s
        progress = 0

        deploy_data = json.loads(deploy_json)

        print deploy_data["deploy_hosts"]
        print deploy_data["share_volume_name"]
        print deploy_data["share_volume_type"]
        print deploy_data["share_volume_count"]

        #while progress < 100:
        #    zone_deploy.update_progress(progress, 'working on deploying')
        #    time.sleep(1)
        #    progress += 10
        #zone_deploy.update_progress(100, 'deployment done!', data='success')

	vc = volumeCreate()
	param1 = {'receiver': deploy_data["deploy_hosts"][0],
		  'hostList': deploy_data["deploy_hosts"],
		  'volType': deploy_data["share_volume_type"],
		  'count': deploy_data["share_volume_count"],
		  'volName': deploy_data["share_volume_name"]
	}
	gluster_param = json.dumps(param1, sort_keys=True, indent=4)
	vc.run(gluster_param)
       
        deploy_hosts = {}
        for host in deploy_data['deploy_hosts']:
            deploy_hosts[host] = 'compute'

        deploy_result = {
            'deploy_hosts': deploy_hosts,
            'dashboard_url': 'http://localhost/'
        }
        result = {
            'result': True,
            'msg': 'Deployment Complete',
            'data': deploy_result
        }
        return result


@task
def init_zone(deploy_json):
    '''
    http://172.16.229.241:3000/projects/rd-001/wiki/Task_Deploy_Framework
    {
        'zone': {
        },
        'parameters': {
            'zone_id': [ '2' ],
            'gluster_vol_name': [ 'test-volume' ],
            'glusterfs_client': [ '0000000001.delta.com' ],
            'glusterfs_client_server': [ '10.129.7.3' ],
            'instance_path': [ '/instance' ],
            'vms_vol_size': [ '6' ],
            'libvirt_type': [ 'qemu' ],
            'network_manager': [ 'nova.network.manager.FlatManager' ],
            'network_interface': [ 'eth0' ],
            'network_ip_range': [ '192.168.2.0/24' ],
            'puppet_server': [ 'ubuntu77.delta.com' ],
            'ssh_user': [ 'nii' ],
            'cli_user': [ 'cii' ],
            'moduletest': [ 'ubuntu19.delta.com' ],
        },
        'install': {
            'nova_api': [ '0000000001.delta.com' ],
            'euca2ools': [ '0000000001.delta.com' ],
            'glusterfs': [ '0000000003.delta.com' ],
            'nova_network': [ '0000000002.delta.com' ],
        },
        'uninstall': {
        },
    }
    '''
    return 'success'


@task
def delete_node(deploy_json):
    '''
    delete node by host name
    sample json:
    {
        "hosts": [
                    "host1",
                    "host2"
                 ]
    }
    '''
    print 'delete node server called'
    print 'json:' + deploy_json
    try:
        ar = json.loads(deploy_json)
        if "hosts" in ar:
            for host in ar["hosts"]:
                print host + " adding...."
                time.sleep(3)
        print "job finish"
    except Exception, e:
        print e
        return 'fail'

    return 'success'


@task
def add_node(deploy_json):
    '''
    add node, setup by host name
    sample input json:
    {
        "hosts": [
                    "host1",
                    "host2"
                 ]
    }
    sample output json:
    {
       result: true/false
       msg: optional msg string for status/result description
       data: optional extra data (in JSON string format) for returning to task caller
    }
    '''
    logging.basicConfig(filename='logs/workers_bootstrapper_add_node.log',level=logging.DEBUG)
    logging.debug('add node server called')
    logging.debug('json:' + deploy_json)

    try:
        ar = json.loads(deploy_json)
        if "hosts" in ar:
            for host in ar["hosts"]:
                logging.debug(host + " adding....")
                time.sleep(1)
        logging.debug("job finish")
    except Exception as e:
        logging.error(str(e))
        d = {'result': False, 'msg': str(e), 'data': str(deploy_json)}
        logging.debug(json.dumps(d))
        logging.debug('fail')
        return json.dumps(d)

    logging.debug('success')
    return 'success'
