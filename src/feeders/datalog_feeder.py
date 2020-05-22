import json
import subprocess
import time
import rospy
from tempfile import NamedTemporaryFile
import ipfshttpclient

from feeders import IFeeder
from stations import StationData, Measurement


def _create_row(m: Measurement) -> dict:
    return {
        "pm25": m.pm25,
        "pm10": m.pm10,
        "geo": "{},{}".format(m.geo_lat, m.geo_lon),
        "teimstamp": m.timestamp
    }

def _sort_payload(data: dict) -> dict:
    ordered = {}
    for k,v in data.items():
        meas = sorted(v["measurements"], key=lambda x: x["teimstamp"])
        ordered[k] = {"model":v["model"], "measurements":meas}

def _get_multihash(buffer: set) -> str:
    payload = {}

    for m in buffer:
        if m.public in payload:
            payload[m.public]["measurements"].append(_create_row(m))
        else:
            payload[m.public] = {
                "model": m.model,
                "measurements": [
                    _create_row(m)
                ]
            }

    payload = _sort_payload(payload)

    temp = NamedTemporaryFile(mode="w", delete=False)
    temp.write(json.dumps(payload))
    temp.close()

    with ipfshttpclient.connect() as client:
        response = client.add(temp.name)
        return response["Hash"]


class DatalogFeeder(IFeeder):
    """
    The feeder is responsible for collecting measurements and
    publishing an IPFS hash to Robonomics on Substrate

    It requires the full path to `robonomics` execution binary
    and an account's private key
    """

    def __init__(self, config):
        super().__init__(config)
        self.last_time = time.time()
        self.buffer = set()
        self.interval = self.config["datalog"]["dump_interval"]

    def feed(self, data: StationData):
        if self.config["datalog"]["enable"]:
            rospy.loginfo("DatalogFeeder:")
            if data.measurement.public:
                self.buffer.add(data.measurement)

            if (time.time() - self.last_time) >= self.interval:
                ipfs_hash = _get_multihash(self.buffer)
                self._to_datalog(ipfs_hash)
                self.buffer = set()
                self.last_time = time.time()
            else:
                rospy.loginfo("Still collecting measurements...")

    def _to_datalog(self, ipfs_hash: str):
        rospy.loginfo(ipfs_hash)
        prog_path = [self.config["datalog"]["path"], "io", "write", "datalog",
                     "-s", self.config["datalog"]["suri"], "--remote", self.config["datalog"]["remote"]]
        output = subprocess.run(prog_path, stdout=subprocess.PIPE, input=ipfs_hash.encode(),
                           stderr=subprocess.PIPE)
        rospy.loginfo(output.stderr)
        rospy.logdebug(output)
